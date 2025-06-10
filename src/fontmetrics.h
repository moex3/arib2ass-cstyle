#ifndef ARIB2ASS_FONTMETRICS_H
#define ARIB2ASS_FONTMETRICS_H
#include "font.h"
#include <stdint.h>
#include <uchar.h>

#include FT_FREETYPE_H

/* This speeds up the generation considerably */
#define FM_CHAR_CACHE 1

/* only width is used */
struct text_extents {
    double x_adv, y_adv, x_off, y_off, width, height;
};

struct fm_ctx {
    struct font *font_ref;
    int fs;

#if FM_CHAR_CACHE == 1
    struct fm_char_cache {
        uint32_t key;
        double value;
    } *char_cache;
#endif
};

void fm_create(struct font *font, struct fm_ctx *out_ctx);
void fm_destroy(struct fm_ctx *ctx);

void fm_get_metrics_cp(struct fm_ctx *ctx, char32_t codepoint, struct text_extents *out);
void fm_get_metrics_fs_cp(struct fm_ctx *ctx, char32_t codepoint, int fs, struct text_extents *out);

int  fm_get_fs(struct fm_ctx *ctx);
void fm_set_fs(struct fm_ctx *ctx, int fs);

int fm_adjust_fs(struct fm_ctx *ctx, int target_fs);

#endif /* ARIB2ASS_FONTMETRICS_H */

