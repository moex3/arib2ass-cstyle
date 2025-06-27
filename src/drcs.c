#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#include "drcs.h"
#include "log.h"
#include "util.h"
#include "platform.h"
#include "opts.h"
#include "png.h"


struct drcs_conv {
    char *key;
    char32_t value;
};
/* Custom DRCS replacing on top of what libaribcaption already does */
static const struct drcs_conv static_replace_map[] = {
    {"063c95566807d5e7b51ab706426bedf9", 0x1F4F1},
    {"06cb56043b9c4006bcfbe07cc831feaf", 0x1F50A},
    {"170193c22e22a88904c34f3ec7129ddb", 0x1F5A5},
    {"33d4c5243a45503d43fbb858a728664d", 0x1F4F1},
    {"3830a0e0148cfb20309ed54d89472156", 0x1F4DE},
    {"4ba716a88c003ca0a069392be3b63951", 0x27A1},
    {"5063561406195ca45f5992e3f7ad77d2", 0xFF5F},
    {"516a7b4eb9de2841903301997e881e9d", 0x1F50A},
    {"56b48663ae06a5545e5b233bb006cdf0", 0x1F4F1},
    {"5c31e7978a711d0ca0469b294cb47ca6", 0x1F50A},
    {"68fc649b4a57a6103a25dc678fcec9f4", 0x1F4F1},
    {"6d5aa3ff99a144bd5138562787f58590", 0x1F4F1},
    {"74d535ca9f47d57fd78234f7019a525e", 0x269F},
    {"7542bc0875d546542d2435daa99821bb", 0x1F4F1},
    {"804a5bcdcbf1ba977c92d3d58a1cdfe1", 0x1F50A},
    {"86aed3fe53ad8f629253795c87452fab", 0x1F4F2},
    {"a368b4ce2212ef80e2bf3d68559f5151", 0x1F4FA},
    {"a97b575907c06f39c8ac0ed26c49e328", 0x1F4F1},
    {"c35c7e6816e10be8304f2d876426c9e7", 0x1F4F1},
    {"c42450812c184aee2ae01cc5e39ae957", 0x1F4FB},
    {"d84fc83615b75802ed422eda4ba39465", 0xFF60},
    {"df055ddbbdbb84d22900081137c070b0", 0x1F5A5},
    {"fe720d2a491d8a4441151c49cd8ab4f6", 0x1F5B3},
};

/* Dynamic hash map of md5sum -> replacement codepoint for
 * drcs values. Overrides static_replace_map */
struct drcs_conv stb_hmap *dyn_replace_map = NULL;

/* Character to use for default replacements
 * 0 if not set */
char32_t default_repl_char = 0;

static void write_file_to_drcs_dir(const char *filename, const uint8_t *data, size_t data_size)
{
    const pchar *subdir = PSTR("drcs");
    pchar fpath[384];
    ssize_t wres;
    int n, fd, dirend;

#ifdef _WIN32
    pchar *curr_dir = get_current_dir_name();
    assert(curr_dir);
#else
    const pchar *curr_dir = ".";
#endif

    n = psnprintf(fpath, sizeof(fpath), PSTR("%s%c%s%n%c%s"), curr_dir, PATHSPECC, subdir, &dirend, PATHSPECC, filename);
    assert(n + 1 < ARRAY_COUNT(fpath));

    assert(fpath[dirend] == PATHSPECC);
    fpath[dirend] = '\0';
    mkdir_p(fpath);
    fpath[dirend] = PATHSPECC;

    fd = plopen(fpath, O_WRONLY | O_CREAT | O_EXCL | _O_BINARY, 0644);
    if (fd == -1) {
        if (errno != EEXIST) {
            n = errno;
            log_error("Failed to open drcs file '%s' (%s)\n", fpath, pstrerror(n));
        }
        /* Either couldn't open file for some reason,
         * or it already exists, so return */
        goto end;
    }

    wres = plwrite(fd, data, data_size);
    if (wres != data_size) {
        log_error("Couldn't write all drcs file data: %s\n", pstrerror(errno));
    }

    plclose(fd);

end:
#ifdef _WIN32
    free(curr_dir);
#endif
}

enum error drcs_write_to_png(aribcc_drcs_t *drcs)
{
    pchar fname[64];
    enum error err;
    int d, depth, w, h, n;
    size_t pxsize, png_data_size;
    uint8_t *png_data, *px;
    const char *md5;

    aribcc_drcs_get_size(drcs, &w, &h);
    aribcc_drcs_get_depth(drcs, &d, &depth),
    aribcc_drcs_get_pixels(drcs, &px, &pxsize);
    md5 = aribcc_drcs_get_md5(drcs);

    err = png_encode(w, h, depth, px, pxsize, &png_data, &png_data_size);
    if (err != NOERR)
        return err;

    n = psnprintf(fname, sizeof(fname), PSTR("%s.png"), u8PC(md5));
    assert(n + 1 < ARRAY_COUNT(fname));

    write_file_to_drcs_dir(fname, png_data, png_data_size);
    free(png_data);
    return NOERR;
}


/* To convert the raw pixel data to an image:
 * convert -depth [depth] -size [width]x[height] gray:drcs.bin out.png
 */
static void drcs_dump_char(aribcc_drcsmap_t *map, const aribcc_caption_char_t *chr)
{
    assert(chr->type == ARIBCC_CHARTYPE_DRCS || chr->type == ARIBCC_CHARTYPE_DRCS_REPLACED);

    pchar fname[64];
    int d, depth, w, h, n;
    size_t pxsize;
    uint8_t *px;
    const char *md5;
    aribcc_drcs_t *dr;
    bool replaced;

    dr = aribcc_drcsmap_get(map, chr->drcs_code);
    assert(dr);

    replaced = (chr->type == ARIBCC_CHARTYPE_DRCS_REPLACED);
    aribcc_drcs_get_size(dr, &w, &h);
    aribcc_drcs_get_depth(dr, &d, &depth),
    aribcc_drcs_get_pixels(dr, &px, &pxsize);
    md5 = aribcc_drcs_get_md5(dr);

    if (replaced == false) {
        for (int i = 0; i < ARRAY_COUNT(static_replace_map); i++) {
            if (strcmp(static_replace_map[i].key, md5) == 0) {
                replaced = true;
                break;
            }
        }
    }

    if (opt_dump_drcs_format == PNG) {
        uint8_t *png_data;
        size_t png_data_size;

        enum error err = png_encode(w, h, depth, px, pxsize, &png_data, &png_data_size);
        if (err != NOERR)
            return;

        n = psnprintf(fname, sizeof(fname), PSTR("%d_%d_%d_%d_%s.png"),
                replaced, w, h, depth, u8PC(md5));
        assert(n + 1 < ARRAY_COUNT(fname));

        write_file_to_drcs_dir(fname, png_data, png_data_size);
        free(png_data);
    } else if (opt_dump_drcs_format == BIN) {
        /* Filename format is
         * REPLACED_WIDTH_HEIGHT_DEPTH_MD5.bin
         */
        n = psnprintf(fname, sizeof(fname), PSTR("%d_%d_%d_%d_%s.bin"),
                replaced, w, h, depth, u8PC(md5));
        assert(n + 1 < ARRAY_COUNT(fname));
        write_file_to_drcs_dir(fname, px, pxsize);
    }
}

enum error drcs_dump(const struct subobj_ctx *s)
{
    for (intptr_t ai = 0; ai < arrlen(s->subobjs); ai++) {
        const aribcc_caption_t *caption = &s->subobjs[ai].caption_ref;

        for (uint32_t ri = 0; ri < caption->region_count; ri++) {
            const aribcc_caption_region_t *region = &caption->regions[ri];

            for (uint32_t ci = 0; ci < region->char_count; ci++) {
                const aribcc_caption_char_t *chr = &region->chars[ci];

                if (chr->type == ARIBCC_CHARTYPE_DRCS || chr->type == ARIBCC_CHARTYPE_DRCS_REPLACED) {
                    drcs_dump_char(caption->drcs_map, chr);
                }
            }
        }
    }
    return NOERR;
}

/* Currently only those, that are not replaced by libaribcaption will
 * be replaced here (could be changed later). So the replacement hierarchy goes
 * libaribcaption -> custom user replacements -> custom static replacements -> 'all' replacement
 */
char32_t drcs_get_replacement_ucs4_by_md5(const char *md5)
{
    const struct drcs_conv *di = shgetp_null(dyn_replace_map, md5);
    if (di) {
        return di->value;
    }

    for (int i = 0; i < ARRAY_COUNT(static_replace_map); i++) {
        if (strcmp(static_replace_map[i].key, md5) == 0) {
            return static_replace_map[i].value;
        }
    }

    if (default_repl_char != 0) {
        return default_repl_char;
    }

    return 0;
}

enum error drcs_add_mapping(const char *md5, char32_t codepoint)
{
    if (md5 != NULL) {
        for (int i = 0; i < 32; i++) {
            if (!(isxdigit(md5[i]))) {
                log_warning("Skipping invalid drcs md5sum: %32s\n", u8PC(md5));
                return ERR_INVALID_DRCS_REPLACEMENT;
            }
        }
    }

    if (opt_log_level <= LOG_DEBUG) {
        char u8[8];
        unicode_to_utf8(codepoint, u8);
#ifdef _WIN32
        pchar wc8[8];
        psnprintf(wc8, ARRAY_COUNT(wc8), PSTR("%s"), u8PC(u8));
        log_debug("Adding DRCS replacement character: %s => %s\n", u8PC(md5 ? md5 : "all"), wc8);
#else
        log_debug("Adding DRCS replacement character: %s => %s\n", md5 ? md5 : "all", u8);
#endif
    }

    if (md5 == NULL) {
        default_repl_char = codepoint;
        return NOERR;
    }

    char *own_md5 = malloc(33);
    assert(own_md5);
    for (int i = 0; i < 32; i++)
        own_md5[i] = tolower((unsigned char)md5[i]);
    own_md5[32] = '\0';
    shput(dyn_replace_map, own_md5, codepoint);

    return NOERR;
}

enum error drcs_add_mapping_u8(const char *md5, const char *u8char)
{
    char32_t c = utf8_to_unicode(u8char);
    if (c == 0) {
        log_warning("Invalid UTF8 drcs replacement character\n");
        return ERR_INVALID_DRCS_REPLACEMENT;
    }

    return drcs_add_mapping(md5, c);
}

void drcs_add_mapping_from_table(const toml_table_t *tbl)
{
    int tlen = toml_table_len(tbl);
    for (int i = 0; i < tlen; i++) {
        toml_value_t val;
        int klen;
        const char *key = toml_table_key(tbl, i, &klen);
        const char *md5 = key;
        if (klen != 32) {
            if ((klen == 1 && strncmp(key, "*", 1) == 0) ||
                    (klen == 3 && strncmp(key, "all", 3) == 0)) {
                md5 = NULL; /* all */
            } else {
                log_debug("Skipping invalid key from drcs_conv table: %s\n", u8PC(key));
                continue;
            }
        }

        val = toml_table_int(tbl, key);
        if (val.ok) {
            drcs_add_mapping(md5, val.u.i);
        } else {
            val = toml_table_string(tbl, key);
            if (val.ok) {
                drcs_add_mapping_u8(md5, val.u.s);
                free(val.u.s);
            } else {
                log_debug("Skipping invalid drcs_conv table value for key: %s\n", u8PC(key));
                continue;
            }
        }
    }
}

void drcs_add_mapping_from_file(const pchar *path)
{
    char errorbuf[256];
    FILE *f = pfopen(path, PSTR("rb"));
    if (f == NULL) {
        log_warning("Failed to open drcs replacement file: '%s': %s\n",
                path, pstrerror(errno));
        return;
    }

	toml_table_t *toml = toml_parse_file(f, errorbuf, sizeof(errorbuf));
    if (toml == NULL) {
        log_error("Failed to parse drcs replacement file %s\n", u8PC(errorbuf));
        fclose(f);
        return;
    }

    drcs_add_mapping_from_table(toml);

    toml_free(toml);
    fclose(f);
}


void drcs_free()
{
    size_t len = shlenu(dyn_replace_map);
    for (size_t i = 0; i < len; i++)
        free((char*)dyn_replace_map[i].key);
    shfree(dyn_replace_map);
}
