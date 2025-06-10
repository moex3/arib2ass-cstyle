#ifndef ARIB2ASS_SUBOBJ_H
#define ARIB2ASS_SUBOBJ_H
#include <time.h>
#include <stdbool.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <aribcaption/aribcaption.h>

#include "error.h"
#include "tsdecode.h"
#include "stb_ds.h"
#include "defs.h"

struct subobj_caption_char {
    union {
        /* This can have both a ref owned by the aribcc decoder, or
         * a new char that is owned by subobj */
        const aribcc_caption_char_t *ref;
        aribcc_caption_char_t *chr;
    };
    bool owned_chr;

    aribcc_color_t text_color;    ///< Color of the text (foreground)
    aribcc_color_t back_color;    ///< Color of the background
    aribcc_color_t stroke_color;  ///< Color of the storke text

    aribcc_charstyle_t style;
    int char_width, char_height;
    float char_horizontal_spacing, char_horizontal_scale;
};

struct subobj_caption_region {
    const aribcc_caption_region_t *ref;

    int x, y, height, width;
    struct subobj_caption_char stb_array *so_chars;
};

struct subobj_caption {
    struct subobj_caption_region stb_array *so_regions;
};

struct subobj {
    /* Start and end times for this subtitle object relative to video begin */
    time_t start_ms, end_ms;

    /* received from the arib decoder, read only */
    aribcc_caption_t caption_ref;
    /* copy of caption from the decoder in a different struct, to allow different
     * fields and modifications. */
    struct subobj_caption so_caption;
};

struct subobj_ctx {
    /* Array of parsed subtitle objects */
    struct subobj stb_array *subobjs;

    /* Arib context and decoder */
    aribcc_context_t *arib_ctx;
    aribcc_decoder_t *arib_decoder;

    /* End of the video timestamp.
     * Used when the last subtitle is with undefinied end time */
    time_t video_end_ms;

    /* Last decoded caption which we need if the current
     * caption has an indefinite wait duration */
    aribcc_caption_t last_caption;
    bool             last_end_time_delayed;
};

enum error subobj_create(struct subobj_ctx *out_sctx, const struct tsdecode *tsd);
void       subobj_destroy(struct subobj_ctx *sctx);
void       subobj_reset_mod(struct subobj_ctx *sctx);

enum error subobj_parse_from_packet(struct subobj_ctx *sctx, AVPacket *packet);

void subobj_caption_region_copy(struct subobj_caption_region *dst, const struct subobj_caption_region *src);
void subobj_caption_regions_free(struct subobj_caption_region *regions);

#endif /* ARIB2ASS_SUBOBJ_H */
