#include "fontmetrics.h"
#include "log.h"
#include <assert.h>
#include <wchar.h>
#include <math.h>
#include FT_TRUETYPE_TABLES_H
#include "stb_ds.h"
#include "util.h"

// https://github.com/libass/libass/blob/ad42889c85fc61a003ad6d4cdb985f56de066f91/libass/ass_font.c#L278
static void set_font_metrics(FT_Face ftface)
{
    TT_OS2 *os2 = FT_Get_Sfnt_Table(ftface, FT_SFNT_OS2);
    if (os2 && ((short)os2->usWinAscent + (short)os2->usWinDescent != 0)) {
        ftface->ascender  =  (short)os2->usWinAscent;
        ftface->descender = -(short)os2->usWinDescent;
        ftface->height    = ftface->ascender - ftface->descender;
    }
    if (ftface->ascender - ftface->descender == 0 || ftface->height == 0) {
        if (os2 && (os2->sTypoAscender - os2->sTypoDescender) != 0) {
            ftface->ascender = os2->sTypoAscender;
            ftface->descender = os2->sTypoDescender;
            ftface->height = ftface->ascender - ftface->descender;
        } else {
            ftface->ascender = ftface->bbox.yMax;
            ftface->descender = ftface->bbox.yMin;
            ftface->height = ftface->ascender - ftface->descender;
        }
    }
}

static void reset_fm_char_cache(struct fm_ctx *c)
{
#if FM_CHAR_CACHE == 1
    hmfree(c->char_cache);
#endif
}

void fm_set_fs(struct fm_ctx *ctx, int fs)
{
    reset_fm_char_cache(ctx);

    FT_Error err;
    set_font_metrics(ctx->font_ref->face);
    FT_Size_RequestRec rq = {
        .type = FT_SIZE_REQUEST_TYPE_REAL_DIM,
        .width = 0,
        .height = lrint(fs * 64),
    };
    err = FT_Request_Size(ctx->font_ref->face, &rq);
    assert(!err);

    ctx->fs = fs;
}

int fm_get_fs(struct fm_ctx *ctx)
{
    return ctx->fs;
}

void fm_create(struct font *font, struct fm_ctx *out_ctx)
{
    FT_Error err;

    err = FT_Select_Charmap(font->face, FT_ENCODING_UNICODE);
    assert(err == FT_Err_Ok);

    *out_ctx = (struct fm_ctx){
        .font_ref = font,
    };
}

void fm_destroy(struct fm_ctx *ctx)
{
    reset_fm_char_cache(ctx);

    memset(ctx, 0, sizeof(*ctx));
}

static void fm_get_metrics_cp_inter(struct fm_ctx *ctx, char32_t codepoint, FT_UInt gid, struct text_extents *out)
{
#if FM_CHAR_CACHE == 1
    ptrdiff_t hi = hmgeti(ctx->char_cache, codepoint);
    if (hi != -1) {
        out->width = ctx->char_cache[hi].value;
        return;
    }
#endif

    if (gid == 0)
        gid = FT_Get_Char_Index(ctx->font_ref->face, codepoint);
    if (gid == 0) {
        pchar mt[8];
        unicode_to_pchar(codepoint, mt);
        log_warning("Selected font is missing character codepoint:'0x%X' (%s)\n", codepoint, mt);
        out->width = ctx->fs;
        goto done;
    }

    FT_Error ferr = FT_Load_Glyph(ctx->font_ref->face, gid, FT_LOAD_DEFAULT);
    assert(ferr == 0);

    out->width = ctx->font_ref->face->glyph->metrics.horiAdvance / 64.f;
done:

#if FM_CHAR_CACHE == 1
    hmput(ctx->char_cache, codepoint, out->width);
#endif
    return;
}

void fm_get_metrics_cp(struct fm_ctx *ctx, char32_t codepoint, struct text_extents *out)
{
    fm_get_metrics_cp_inter(ctx, codepoint, 0, out);
}

void fm_get_metrics_fs_cp(struct fm_ctx *ctx, char32_t codepoint, int fs, struct text_extents *out)
{
    if (fs != fm_get_fs(ctx)) {
        fm_set_fs(ctx, fs);
    }
    fm_get_metrics_cp(ctx, codepoint, out);
}

int fm_adjust_fs(struct fm_ctx *ctx, int target_fs)
{
    static const char32_t chr = 0x5B57; // 字
    int newfs = target_fs;
    struct text_extents ex;
    int adj = 0;
    for (;;) {
        fm_get_metrics_fs_cp(ctx, chr, newfs, &ex);
        int aw = (int)ex.width;

        if (aw == target_fs)
            return newfs;

        if (aw > target_fs)
            return newfs - adj;

        if (adj == 0) {
            assert(aw < target_fs);
            adj = 1;
        }

        newfs += adj;
    }
}
