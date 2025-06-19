#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "drcs.h"
#include "log.h"
#include "util.h"
#include "opts.h"
#include "platform.h"

struct drcs_map {
    const char *md5;
    uint32_t ucs4;
};
/* Custom DRCS replacing on top of what libaribcaption already does */
static const struct drcs_map drcs_replace_map[] = {
    {"063c95566807d5e7b51ab706426bedf9", 0x1F4F1},
    {"06cb56043b9c4006bcfbe07cc831feaf", 0x1F50A},
    {"3830a0e0148cfb20309ed54d89472156", 0x1F4DE},
    {"df055ddbbdbb84d22900081137c070b0", 0x1F5A5},
    {"68fc649b4a57a6103a25dc678fcec9f4", 0x1F4F1},
    {"804a5bcdcbf1ba977c92d3d58a1cdfe1", 0x1F50A},
    {"5063561406195ca45f5992e3f7ad77d2", 0xFF5F},
    {"d84fc83615b75802ed422eda4ba39465", 0xFF60},
    {"4ba716a88c003ca0a069392be3b63951", 0x27A1},
    {"fe720d2a491d8a4441151c49cd8ab4f6", 0x1F5B3},
};


struct drcs_dump_info {
    int w, h, depth;
    uint8_t *px;
    size_t pxsize;
    const char *md5;
    bool replaced;
};
static void write_drcs_to_file(const struct drcs_dump_info *di)
{
    const pchar *subdir = PSTR("drcs");
    pchar fpath[384];
    ssize_t wres;
    int n, fd;

#ifdef _WIN32
    pchar *curr_dir = get_current_dir_name();
    assert(curr_dir);
#else
    const pchar *curr_dir = ".";
#endif

    n = psnprintf(fpath, sizeof(fpath), PSTR("%s%c%s"), curr_dir, PATHSPECC, subdir);
    assert(n + 1 < ARRAY_COUNT(fpath));

    mkdir_p(fpath);

    /* Filename format is
     * REPLACED_WIDTH_HEIGHT_DEPTH_MD5.bin
     */
    n = psnprintf(fpath, sizeof(fpath), PSTR("%s%c%s%c%d_%d_%d_%d_%s.bin"),
            curr_dir, PATHSPECC, subdir, PATHSPECC,
            di->replaced, di->w, di->h, di->depth, u8PC(di->md5));
    assert(n + 1 < ARRAY_COUNT(fpath));

    fd = plopen(fpath, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd == -1) {
        if (errno != EEXIST) {
            n = errno;
            log_error("Failed to open drcs file '%s' (%s)\n", fpath, pstrerror(n));
        }
        /* Either couldn't open file for some reason,
         * or it already exists, so return */
        goto end;
    }

    wres = plwrite(fd, di->px, di->pxsize);
    if (wres != di->pxsize) {
        log_error("Couldn't write all drcs pixel data: %s\n", pstrerror(errno));
    }

    plclose(fd);

end:
#ifdef _WIN32
    free(curr_dir);
#endif
}


/* To convert the raw pixel data to an image:
 * convert -depth [depth] -size [width]x[height] gray:drcs.bin out.png
 */
static void drcs_dump_char(aribcc_drcsmap_t *map, const aribcc_caption_char_t *chr)
{
    assert(chr->type == ARIBCC_CHARTYPE_DRCS || chr->type == ARIBCC_CHARTYPE_DRCS_REPLACED);

    struct drcs_dump_info di = {0};
    int d;
    aribcc_drcs_t *dr;

    dr = aribcc_drcsmap_get(map, chr->drcs_code);
    assert(dr);

    di.replaced = (chr->type == ARIBCC_CHARTYPE_DRCS_REPLACED);
    aribcc_drcs_get_size(dr, &di.w, &di.h);
    aribcc_drcs_get_depth(dr, &d, &di.depth),
    aribcc_drcs_get_pixels(dr, &di.px, &di.pxsize);
    di.md5 = aribcc_drcs_get_md5(dr);

    if (di.replaced == false) {
        for (int i = 0; i < ARRAY_COUNT(drcs_replace_map); i++) {
            if (strcmp(drcs_replace_map[i].md5, di.md5) == 0) {
                di.replaced = true;
                break;
            }
        }
    }

    write_drcs_to_file(&di);
}

enum error drcs_dump(const struct subobj_ctx *s)
{
    for (intptr_t ai = 0; ai < arrlen(s->subobjs); ai++) {
        const aribcc_caption_t *caption = &s->subobjs[ai].caption_ref;

        for (uint32_t ri = 0; ri < caption->region_count; ri++) {
            const aribcc_caption_region_t *region = &caption->regions[ri];

            for (uint32_t ci = 0; ci < region->char_count; ci++) {
                const aribcc_caption_char_t *chr = &region->chars[ci];

                if (chr->type == ARIBCC_CHARTYPE_DRCS || chr->type == ARIBCC_CHARTYPE_DRCS_REPLACED)
                    drcs_dump_char(caption->drcs_map, chr);
            }
        }
    }
    return NOERR;
}

void drcs_replace(aribcc_drcsmap_t *drcs_map, aribcc_caption_char_t *chr)
{
    /* libaribcaption does not support adding new or custom drcs mappings.
     * maybe i should add it?
     * anyway, hack it here for now */
    assert(chr->type == ARIBCC_CHARTYPE_DRCS);

    aribcc_drcs_t *d = aribcc_drcsmap_get(drcs_map, chr->drcs_code);
    assert(d);
    const char *md5 = aribcc_drcs_get_md5(d);
    for (int i = 0; i < ARRAY_COUNT(drcs_replace_map); i++) {
        if (strcmp(drcs_replace_map[i].md5, md5) == 0) {
            chr->type = ARIBCC_CHARTYPE_DRCS_REPLACED;
            chr->codepoint = drcs_replace_map[i].ucs4;
            unicode_to_utf8(drcs_replace_map[i].ucs4, chr->u8str);
#ifdef _WIN32
            pchar replstr[8];
            _snwprintf(replstr, ARRAY_COUNT(replstr), L"%s", u8PC(chr->u8str));
            log_info("Replaced drcs %s to %s\n", u8PC(md5), replstr);
#else
            log_info("Replaced drcs %s to %s\n", md5, chr->u8str);
#endif
            return;
        }
    }
    log_warning("Found no drcs replace char for %s\n", u8PC(md5));

}
