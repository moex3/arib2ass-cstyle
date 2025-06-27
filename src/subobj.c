#include "subobj.h"
#include <assert.h>
#include <fcntl.h>

#include <stdio.h>
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "log.h"
#include "opts.h"
#include "util.h"
#include "drcs.h"

static void arib_log_cb(aribcc_loglevel_t level, const char* message, void* userdata)
{
    log_info("[libaribcaption-%d] %s\n", level, u8PC(message));
}

enum error subobj_create(struct subobj_ctx *out_sctx, const struct tsdecode *tsd)
{
    bool              arib_res;
    enum error        err  = ERR_UNDEF;
    aribcc_context_t *actx = NULL;
    aribcc_decoder_t *adec = NULL;

    actx = aribcc_context_alloc();
    if (actx == NULL) {
        err = ERR_ARIBCC;
        goto fail;
    }
    aribcc_context_set_logcat_callback(actx, arib_log_cb, NULL);

    adec = aribcc_decoder_alloc(actx);
    if (adec == NULL) {
        err = ERR_ARIBCC;
        goto fail;
    }
    arib_res = aribcc_decoder_initialize(adec, ARIBCC_ENCODING_SCHEME_AUTO, ARIBCC_CAPTIONTYPE_CAPTION, ARIBCC_PROFILE_DEFAULT, ARIBCC_LANGUAGEID_DEFAULT);
    if (arib_res == false) {
        err = ERR_ARIBCC;
        goto fail;
    }

    *out_sctx = (struct subobj_ctx){
        .subobjs      = NULL,
        .arib_ctx     = actx,
        .arib_decoder = adec,
        .video_end_ms = tsdecode_get_video_length(tsd),
    };
    return NOERR;

fail:
    if (adec)
        aribcc_decoder_free(adec);
    if (actx)
        aribcc_context_free(actx);
    return err;
}

static void subobj_caption_region_free(struct subobj_caption_region *region)
{
    for (struct subobj_caption_char *chr = region->so_chars; chr < arrendptr(region->so_chars); chr++) {
        if (chr->owned_chr)
            free(chr->chr);
    }
    arrfree(region->so_chars);
}

void subobj_caption_regions_free(struct subobj_caption_region stb_array *so_regions)
{
    if (so_regions == NULL)
        return;

    for (struct subobj_caption_region *region = so_regions; region < arrendptr(so_regions); region++) {
        subobj_caption_region_free(region);
    }
    arrfree(so_regions);
}

static void subobj_caption_free(struct subobj_caption *so_caption)
{
    subobj_caption_regions_free(so_caption->so_regions);
}

void subobj_caption_region_copy(struct subobj_caption_region *dst, const struct subobj_caption_region *src)
{
    *dst = *src;
    dst->so_chars = NULL;
    arrsetcap(dst->so_chars, arrlen(src->so_chars));
    for (const struct subobj_caption_char *chr = src->so_chars; chr < arrendptr(src->so_chars); chr++) {
        arrput(dst->so_chars, *chr);
    }
}

static void aribcc_caption_char_copy_to_so(struct subobj_caption_char *dst, const aribcc_caption_char_t *src)
{
    *dst = (struct subobj_caption_char){
        .ref = src,
        .owned_chr = false,
        .text_color = src->text_color,
        .back_color = src->back_color,
        .stroke_color = src->stroke_color,
        .style = src->style,
        .char_width = src->char_width,
        .char_height = src->char_height,
        .char_horizontal_scale = src->char_horizontal_scale,
        .char_horizontal_spacing = src->char_horizontal_spacing,
    };
}

static void replace_drcs(aribcc_drcsmap_t *drmap, aribcc_caption_char_t *chr)
{
    assert(chr->type == ARIBCC_CHARTYPE_DRCS);

    aribcc_drcs_t *drcs = aribcc_drcsmap_get(drmap, chr->drcs_code);
    assert(drcs);

    const char *md5 = aribcc_drcs_get_md5(drcs);
    assert(md5);

    chr->type = ARIBCC_CHARTYPE_DRCS_REPLACED;
    char32_t rc = drcs_get_replacement_ucs4_by_md5(md5);
    /* Found */
    if (rc != 0) {
        chr->codepoint = rc;
        unicode_to_utf8(rc, chr->u8str);

        if (opt_log_level <= LOG_INFO) {
#ifdef _WIN32
            pchar replstr[8];
            _snwprintf(replstr, ARRAY_COUNT(replstr), L"%s", u8PC(chr->u8str));
            log_info("Replaced drcs %s to %s\n", u8PC(md5), replstr);
#else
            log_info("Replaced drcs %s to %s\n", md5, chr->u8str);
#endif
        }
        return;
    }

    if (opt_dump_drcs == true && (opt_srt_do == false && opt_ass_do == false)) {
        /* If we are already dumping all of the drcs, don't output the warning */
        return;
    }

    /* Not found */
    log_warning("Found no drcs replacement char for %s. Writing image to file.\n", u8PC(md5));

    /* full width space character, as normal space won't get
     * rendered at the beginning of the line */
    chr->codepoint = 0x3000;
    chr->u8str[0] = 0xE3;
    chr->u8str[1] = 0x80;
    chr->u8str[2] = 0x80;
    chr->u8str[3] = '\0';

    drcs_write_to_png(drcs);
}

static void aribcc_caption_region_copy_to_so_drcs_replace(struct subobj_caption_region *dst, aribcc_caption_region_t *src, aribcc_drcsmap_t *drmap)
{
    *dst = (struct subobj_caption_region){
        .ref = src,
        .x = src->x,
        .y = src->y,
        .height = src->height,
        .width = src->width,
        .so_chars = NULL,
    };
    arrsetcap(dst->so_chars, src->char_count);

    for (uint32_t j = 0; j < src->char_count; j++) {
        struct subobj_caption_char  *dst_char = arraddnptr(dst->so_chars, 1);
        aribcc_caption_char_t *src_char = &src->chars[j];

        if (src_char->type == ARIBCC_CHARTYPE_DRCS)
            replace_drcs(drmap, src_char);

        aribcc_caption_char_copy_to_so(dst_char, src_char);
    }
}

static bool is_region_only_whitespace(const aribcc_caption_region_t *region)
{
    for (uint32_t ci = 0; ci < region->char_count; ci++) {
        const aribcc_caption_char_t *chr = &region->chars[ci];

        /* ' ' and '　' */
        if (!(  strcmp(chr->u8str, u8"\x20") == 0 ||
                strcmp(chr->u8str, u8"\u3000") == 0)) {
            return false;
        }
    }
    return true;
}

static void aribcc_caption_copy_to_so(struct subobj_caption *dst, aribcc_caption_t *src)
{
    dst->so_regions = NULL;
    arrsetcap(dst->so_regions, src->region_count);

    for (uint32_t i = 0; i < src->region_count; i++) {
        aribcc_caption_region_t *src_region = &src->regions[i];
        struct subobj_caption_region  *dst_region;

        /* Skip regions that only has whitespace */
        if (is_region_only_whitespace(src_region))
            continue;
        dst_region = arraddnptr(dst->so_regions, 1);

        aribcc_caption_region_copy_to_so_drcs_replace(dst_region, src_region, src->drcs_map);
    }
}

void subobj_destroy(struct subobj_ctx *sctx)
{
    if (sctx->subobjs) {
        for (intptr_t i = 0; i < arrlen(sctx->subobjs); i++) {
            //text_section_free(sctx->subobjs[i].sections);
            subobj_caption_free(&sctx->subobjs[i].so_caption);
            aribcc_caption_cleanup((aribcc_caption_t*)&sctx->subobjs[i].caption_ref);
        }
        arrfree(sctx->subobjs);
    }

    if (sctx->arib_decoder)
        aribcc_decoder_free(sctx->arib_decoder);
    if (sctx->arib_ctx)
        aribcc_context_free(sctx->arib_ctx);

    memset(sctx, 0, sizeof(*sctx));
}

void subobj_reset_mod(struct subobj_ctx *sctx)
{
    for (intptr_t i = 0; i < arrlen(sctx->subobjs); i++) {
        subobj_caption_free(&sctx->subobjs[i].so_caption);

        aribcc_caption_copy_to_so(&sctx->subobjs[i].so_caption, &sctx->subobjs[i].caption_ref);
    }
}

enum error subobj_parse_from_packet(struct subobj_ctx *sctx, AVPacket *packet)
{
    struct subobj new = {0};

    aribcc_decode_status_t  decode_result;

    decode_result = aribcc_decoder_decode(sctx->arib_decoder, packet->data, packet->size, packet->pts, &new.caption_ref);
    if (decode_result == ARIBCC_DECODE_STATUS_ERROR) {
        log_debug("Error while decoding packet\n");
        return ERR_LIBAV;
    } else if (decode_result == ARIBCC_DECODE_STATUS_NO_CAPTION) {
        return NOERR;
    }
    assert(new.caption_ref.flags & ARIBCC_CAPTIONFLAGS_CLEARSCREEN);
    assert(new.caption_ref.type & ARIBCC_CAPTIONTYPE_SUPERIMPOSE);

    aribcc_caption_copy_to_so(&new.so_caption, &new.caption_ref);

    if (sctx->last_end_time_delayed) {
        sctx->subobjs[arrlen(sctx->subobjs) - 1].end_ms = new.caption_ref.pts;

        sctx->last_end_time_delayed = false;
    }

    new.start_ms = new.caption_ref.pts;
    if (new.caption_ref.flags & ARIBCC_CAPTIONFLAGS_WAITDURATION) {
        new.end_ms = new.start_ms + new.caption_ref.wait_duration;
    } else {
        /* Always set the end time to the end of the video, as
         * this might be the last subtitle line */
        sctx->last_end_time_delayed = true;
    }

    arrput(sctx->subobjs, new);
    return NOERR;
}
