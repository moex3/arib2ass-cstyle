#include "tagtext.h"

#include <stdio.h>
#include <assert.h>
#include "stb_ds.h"
#include "util.h"

struct tagtext_ctx {

    bool optimize;
    struct tagtext_event most_common_styles[TT_STYLE_COUNT_];
};

static void tagtext_events_free(struct tagtext_event stb_array *events)
{
    arrfree(events);
}

static void tagtexts_free(struct tagtext stb_array *tagtexts)
{
    if (tagtexts == NULL)
        return;
    for (struct tagtext *tt = tagtexts; tt < &tagtexts[arrlen(tagtexts)]; tt++) {
        tagtext_events_free(tt->events);
    }
    arrfree(tagtexts);
}

#if 0
static enum error parse_chars_as_sections(const aribcc_caption_region_t *region, struct text_section **out_sec_arr)
{
    struct text_section new_section = {
        .is_ruby = region->is_ruby,
    };
    for (uint32_t ci = 0; ci < region->char_count; ci++) {
        struct text_stream_event current_styles[TT_STYLE_COUNT_];
        struct text_stream new_text = {0};
        struct text_stream_event new_text_event = {
            .type = EVENT_TYPE_TEXT,
        };
        const aribcc_caption_char_t *chr = &region->chars[ci];
        assert(chr->type != ARIBCC_CHARTYPE_DRCS);

        all_styles_from_char(chr, current_styles);
        for (int i = 0; i < TT_STYLE_COUNT_; i++) {
            arrput(new_text.events, current_styles[i]);
        }

        printf("Char: %s  scale x,y: %.02f,%.02f  x:%d y:%d  w:%d h:%d  sx:%d sy:%d\n",
                chr->u8str, chr->char_horizontal_scale, chr->char_vertical_scale, chr->x, chr->y, chr->char_width, chr->char_height, chr->char_horizontal_spacing, chr->char_vertical_spacing);

        for (size_t i = 0; i < strlen(chr->u8str); i++) {
            arrput(new_text_event.text, chr->u8str[i]);
        }
        arrput(new_text_event.text, '\0');

        arrput(new_text.events, new_text_event);

        new_section.x = chr->x;
        new_section.y = chr->y;
        new_section.width = chr->char_width;
        new_section.height = chr->char_width;
        new_section.text = new_text;
        arrput(*out_sec_arr, new_section);
    }

    return NOERR;
}
#endif

void tagtext_update_current_styles(const struct tagtext_event *event, struct tagtext_event out_styles[TT_STYLE_COUNT_])
{
    assert(event->type == TT_EVENT_TYPE_STYLE);

    out_styles[event->style] = *event;
}

struct tagtext_event *tagtext_search_style_for_char(const struct tagtext_event *events, intptr_t event_idx, enum tagtext_style style)
{
    intptr_t chr_idx = event_idx;
    for (; chr_idx < arrlen(events); chr_idx++) {
        if (events[chr_idx].type == TT_EVENT_TYPE_CHAR)
            break;
    }
    assert(chr_idx < arrlen(events));

    for (intptr_t i = chr_idx - 1; i >= 0; i--) {
        if (events[i].type == TT_EVENT_TYPE_CHAR)
            break;

        assert(events[i].type == TT_EVENT_TYPE_STYLE);
        if (events[i].style == style) {
            return (void*)&events[i];
        }
    }
    return NULL;
}

static void all_styles_from_char(const struct subobj_caption_char *chr, struct tagtext_event out_events[TT_STYLE_COUNT_])
{
    memset(out_events, 0, sizeof(*out_events) * TT_STYLE_COUNT_);

#define evs(styl, style_val_member, val) {\
        out_events[styl].type = TT_EVENT_TYPE_STYLE; \
        out_events[styl].style = styl; \
        out_events[styl].style_val_member = (val); \
    }

    evs(TT_STYLE_TEXT_COLOR, style_value_u32, chr->text_color);
    evs(TT_STYLE_BACK_COLOR, style_value_u32, chr->back_color);
    evs(TT_STYLE_STROKE_COLOR, style_value_u32, chr->stroke_color);
    evs(TT_STYLE_SCALE_X, style_value_float, chr->char_horizontal_scale);
    evs(TT_STYLE_SCALE_Y, style_value_float, chr->ref->char_vertical_scale);
    evs(TT_STYLE_SPACING_X, style_value_float, chr->char_horizontal_spacing);
    evs(TT_STYLE_SPACING_Y, style_value_u32, chr->ref->char_vertical_spacing);
    evs(TT_STYLE_CHAR_HEIGHT, style_value_u32, chr->char_height);
    evs(TT_STYLE_CHAR_WIDTH, style_value_u32, chr->ref->char_width);

    evs(TT_STYLE_BOLD, style_value_bool, !!(chr->style & ARIBCC_CHARSTYLE_BOLD));
    evs(TT_STYLE_ITALIC, style_value_bool, !!(chr->style & ARIBCC_CHARSTYLE_ITALIC));
    evs(TT_STYLE_UNDERLINE, style_value_bool, !!(chr->style & ARIBCC_CHARSTYLE_UNDERLINE));
    evs(TT_STYLE_STROKE, style_value_bool, !!(chr->style & ARIBCC_CHARSTYLE_STROKE));

    //evs(TT_STYLE_POS_XY, style_value_u32, (chr->x << 16) | (chr->y & 0xffff));

#undef evs
}

/* Returns the number of different events put in changed_styles */
static int changed_styles_from_char(const struct subobj_caption_char *chr,
        struct tagtext_event current_styles[TT_STYLE_COUNT_], struct tagtext_event changed_styles[TT_STYLE_COUNT_])
{
    int newi = 0;

#define evs(styl, style_val_member, val) {\
        assert(current_styles[styl].type == TT_EVENT_TYPE_STYLE && current_styles[styl].style == styl); \
        if (current_styles[styl].style_val_member != (val)) {\
            changed_styles[newi].type = TT_EVENT_TYPE_STYLE; \
            changed_styles[newi].style = styl; \
            changed_styles[newi].style_val_member = (val); \
            newi++; \
        } \
    }

    evs(TT_STYLE_TEXT_COLOR, style_value_u32, chr->text_color);
    evs(TT_STYLE_BACK_COLOR, style_value_u32, chr->back_color);
    evs(TT_STYLE_STROKE_COLOR, style_value_u32, chr->stroke_color);
    evs(TT_STYLE_SCALE_X, style_value_float, chr->char_horizontal_scale);
    evs(TT_STYLE_SCALE_Y, style_value_float, chr->ref->char_vertical_scale);
    evs(TT_STYLE_SPACING_X, style_value_float, chr->char_horizontal_spacing);
    evs(TT_STYLE_SPACING_Y, style_value_u32, chr->ref->char_vertical_spacing);
    evs(TT_STYLE_CHAR_HEIGHT, style_value_u32, chr->char_height);
    evs(TT_STYLE_CHAR_WIDTH, style_value_u32, chr->ref->char_width);

    evs(TT_STYLE_BOLD, style_value_bool, !!(chr->style & ARIBCC_CHARSTYLE_BOLD));
    evs(TT_STYLE_ITALIC, style_value_bool, !!(chr->style & ARIBCC_CHARSTYLE_ITALIC));
    evs(TT_STYLE_UNDERLINE, style_value_bool, !!(chr->style & ARIBCC_CHARSTYLE_UNDERLINE));
    evs(TT_STYLE_STROKE, style_value_bool, !!(chr->style & ARIBCC_CHARSTYLE_STROKE));

    //evs(TT_STYLE_POS_XY, style_value_u32, (chr->x << 16) | (chr->y & 0xffff));

#undef evs

    return newi;
}

static bool textevent_style_cmp(const struct tagtext_event *a, const struct tagtext_event *b)
{
    if (a->type != TT_EVENT_TYPE_STYLE || b->type != TT_EVENT_TYPE_STYLE)
        return false;
    if (a->style != b->style)
        return false;


    switch (a->style) {
        case TT_STYLE_TEXT_COLOR:
        case TT_STYLE_BACK_COLOR:
        case TT_STYLE_STROKE_COLOR:
        case TT_STYLE_SPACING_Y:
        case TT_STYLE_CHAR_HEIGHT:
        case TT_STYLE_CHAR_WIDTH:
            return a->style_value_u32 == b->style_value_u32;

        case TT_STYLE_SCALE_X:
        case TT_STYLE_SCALE_Y:
        case TT_STYLE_SPACING_X:
            return a->style_value_float == b->style_value_float;

        case TT_STYLE_BOLD:
        case TT_STYLE_ITALIC:
        case TT_STYLE_UNDERLINE:
        case TT_STYLE_STROKE:
            return a->style_value_bool == b->style_value_bool;
        default:
            return false;
    }
}



static enum error parse_so_chars_to_events(struct tagtext_ctx *ctx, const struct subobj_caption_char stb_array *chrs, struct tagtext_event *stb_array *out_events)
{
    struct tagtext_event current_styles[TT_STYLE_COUNT_];
    struct tagtext_event new_text_event;
    const struct subobj_caption_char *chr = &chrs[0];

    assert(arrlen(chrs) > 0);

    assert(chr->ref->type != ARIBCC_CHARTYPE_DRCS);

    //printf("Char: %s  scale x,y: %.02f,%.02f  x:%d y:%d  w:%d h:%d  sx:%d sy:%d\n",
            //chr->u8str, chr->char_horizontal_scale, chr->char_vertical_scale, chr->x, chr->y, chr->char_width, chr->char_height, chr->char_horizontal_spacing, chr->char_vertical_spacing);

    all_styles_from_char(chr, current_styles);
    for (int i = 0; i < ARRAY_COUNT(current_styles); i++) {
        if (ctx->optimize) {
            if (textevent_style_cmp(&ctx->most_common_styles[current_styles[i].style], &current_styles[i]) == true)
                /* The style for this char is in the default style, so don't output a style event for it */
                continue;
        }
        arrput(*out_events, current_styles[i]);
    }

    new_text_event = (struct tagtext_event){
        .type = TT_EVENT_TYPE_CHAR,
        .ref_so_chr = chr,
    };
    arrput(*out_events, new_text_event);

    for (chr = &chrs[1]; chr < arrendptr(chrs); chr++) {
        struct tagtext_event changed_styles[TT_STYLE_COUNT_];
        int changed_styles_count;

        assert(chr->ref->type != ARIBCC_CHARTYPE_DRCS);
        //printf("Char: %s  scale x,y: %.02f,%.02f  x:%d y:%d  w:%d h:%d  sx:%d sy:%d\n",
                //chr->u8str, chr->char_horizontal_scale, chr->char_vertical_scale, chr->x, chr->y, chr->char_width, chr->char_height, chr->char_horizontal_spacing, chr->char_vertical_spacing);

        /* Check for new styles */
        changed_styles_count = changed_styles_from_char(chr, current_styles, changed_styles);

        for (int i = 0; i < changed_styles_count; i++) {
            current_styles[changed_styles[i].style].style_value = changed_styles[i].style_value;
        }
        bool reset_style = true;
        for (int i = 0; i < TT_STYLE_COUNT_; i++) {
            if (textevent_style_cmp(&current_styles[i], &ctx->most_common_styles[i]) == false) {
                reset_style = false;
                break;
            }
        }
        if (reset_style == false) {
            for (int i = 0; i < changed_styles_count; i++) {
                arrput(*out_events, changed_styles[i]);
            }
        } else if (changed_styles_count > 0) {
            struct tagtext_event reset_event = {
                .type = TT_EVENT_TYPE_STYLE,
                .style = TT_STYLE_RESET,
            };
            arrput(*out_events, reset_event);
        }

        new_text_event = (struct tagtext_event){
            .type = TT_EVENT_TYPE_CHAR,
            .ref_so_chr = chr,
        };
        arrput(*out_events, new_text_event);

    }

    /* if in case of error, this should free *out_events, but it cannot fail, so w/e */
    return NOERR;
}


static enum error parse_so_region(struct tagtext_ctx *ctx, const struct subobj_caption_region *region, struct tagtext *out_tagtext)
{
    struct tagtext_event *new_events = NULL;
    enum error err;

    //printf("Region x:%d y:%d  w:%d h:%d\n", region->x, region->y, region->width, region->height);

    err = parse_so_chars_to_events(ctx, region->so_chars, &new_events);
    if (err != NOERR) {
        return err;
    }

    out_tagtext->events = new_events;

    return err;
}

struct style_freq {
    uintptr_t key;
    uint64_t value;
};

static void tagtext_optimize_styles(struct tagtext_ctx *ctx, const struct subobj stb_array *subobjs)
{
    /* an array of hashmaps for each style with the style value as key,
     * and style value count as value */
    struct style_freq* freqs[TT_STYLE_COUNT_] = {0};

    for (const struct subobj *s = subobjs; s < &subobjs[arrlen(subobjs)]; s++) {
        for (intptr_t regi = 0; regi < arrlen(s->so_caption.so_regions); regi++) {
            const struct subobj_caption_region *region = &s->so_caption.so_regions[regi];

            for (intptr_t ci = 0; ci < arrlen(region->so_chars); ci++) {
                const struct subobj_caption_char *chr = &region->so_chars[ci];
                struct tagtext_event char_styles[TT_STYLE_COUNT_];

                all_styles_from_char(chr, char_styles);
                for (int si = 0; si < TT_STYLE_COUNT_; si++) {
                    struct style_freq **cf = &freqs[char_styles[si].style];

                    ptrdiff_t idx = hmgeti(*cf, char_styles[si].style_value);
                    if (idx != -1) {
                        (*cf)[idx].value++;
                    } else {
                        hmput(*cf, char_styles[si].style_value, 1);
                    }
                }
            }
        }
    }

    for (int styleidx = 0; styleidx < TT_STYLE_COUNT_; styleidx++) {
        ptrdiff_t uniq_val_count = shlen(freqs[styleidx]);
        assert(uniq_val_count >= 1);

        uintptr_t max_style_value = freqs[styleidx][0].key;
        uint64_t  max_style_count = freqs[styleidx][0].value;
        for (ptrdiff_t vi = 1; vi < uniq_val_count; vi++) {
            if (freqs[styleidx][vi].value > max_style_count) {
                max_style_value = freqs[styleidx][vi].key;
                max_style_count = freqs[styleidx][vi].value;
            }
        }

        ctx->most_common_styles[styleidx] = (struct tagtext_event){
            .type = TT_EVENT_TYPE_STYLE,
            .style = (enum tagtext_style)styleidx,
            .style_value = max_style_value,
        };
    }
    ctx->optimize = true;

    for (int i = 0; i < TT_STYLE_COUNT_; i++) {
        shfree(freqs[i]);
    }
}

enum error tagtext_parse_captions(const struct subobj *subobjs, struct tagtext_caption **out_tt_captions,
        struct tagtext_event out_default_styles[TT_STYLE_COUNT_])
{
    enum error err = NOERR;
    struct tagtext_ctx ctx = {0};
    assert(*out_tt_captions == NULL);

    if (out_default_styles) {
        tagtext_optimize_styles(&ctx, subobjs);
        memcpy(out_default_styles, ctx.most_common_styles, sizeof(struct tagtext_event) * TT_STYLE_COUNT_);
    }

    arrsetcap(*out_tt_captions, arrlen(subobjs));

    for (const struct subobj *s = subobjs; s < &subobjs[arrlen(subobjs)]; s++) {
        struct tagtext_caption new_tt_caption = {0};

        new_tt_caption.ref_subobj = s;
        if (arrlen(s->so_caption.so_regions) > 0)
            arrsetcap(new_tt_caption.tagtexts, arrlen(s->so_caption.so_regions));

        for (intptr_t regi = 0; regi < arrlen(s->so_caption.so_regions); regi++) {
            const struct subobj_caption_region *region = &s->so_caption.so_regions[regi];
            struct tagtext *result_tagtext = arraddnptr(new_tt_caption.tagtexts, 1);

            err = parse_so_region(&ctx, region, result_tagtext);
            if (err != NOERR) {
                (void)arrpop(new_tt_caption.tagtexts);
                tagtexts_free(new_tt_caption.tagtexts);
                break;
            }
        }
        if (err != NOERR) {
            break;
        }

        arrput(*out_tt_captions, new_tt_caption);
    }
    if (err != NOERR) {
        tagtext_captions_free(*out_tt_captions);
    }

    return err;
}

void tagtext_captions_free(struct tagtext_caption stb_array *tt_captions)
{
    for (struct tagtext_caption *tt = tt_captions; tt < &tt_captions[arrlen(tt_captions)]; tt++) {
        tagtexts_free(tt->tagtexts);
    }
    arrfree(tt_captions);
}
