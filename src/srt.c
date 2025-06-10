#include "srt.h"
#include "util.h"
#include "opts.h"
#include "tagtext.h"
#include <stdio.h>
#include <assert.h>

struct srt_ctx {
    int64_t linenum;
};

static void ms_to_str(int64_t tms, char out[32])
{
    int64_t h, m, s, ms;
    h = tms / H_IN_MS;
    tms %= H_IN_MS;

    m = tms / M_IN_MS;
    tms %= M_IN_MS;

    s = tms / S_IN_MS;
    ms = tms % S_IN_MS;

    snprintf(out, 32, "%02"PRId64":%02"PRId64":%02"PRId64",%03"PRId64, h, m, s, ms);
}

static void render_line_header(struct srt_ctx *sc, const struct tagtext_caption *caption, FILE *f)
{
    char start_str[32], end_str[32];
    ms_to_str(caption->ref_subobj->start_ms, start_str);
    ms_to_str(caption->ref_subobj->end_ms, end_str);
    sc->linenum++;
    fprintf(f, "%"PRId64"\n%s --> %s\n", sc->linenum, start_str, end_str);

}

static void render_tagtext_event_style(const struct tagtext_event *ev, bool first, FILE *f)
{
    assert(ev->type == TT_EVENT_TYPE_STYLE);
    char tc;

    switch (ev->style) {
        case TT_STYLE_TEXT_COLOR:
            fprintf(f, "<font color=\"#%02x%02x%02x\">",
                    ARIBCC_COLOR_R(ev->style_value_u32), ARIBCC_COLOR_G(ev->style_value_u32), ARIBCC_COLOR_B(ev->style_value_u32));
            return;
        case TT_STYLE_BACK_COLOR:
        case TT_STYLE_STROKE_COLOR:
        case TT_STYLE_SCALE_X:
        case TT_STYLE_SCALE_Y:
        case TT_STYLE_SPACING_X:
        case TT_STYLE_SPACING_Y:
        case TT_STYLE_CHAR_HEIGHT:
        case TT_STYLE_STROKE:
        case TT_STYLE_RESET:
        default:
            return;

        case TT_STYLE_BOLD:
            tc = 'b';
            break;
        case TT_STYLE_ITALIC:
            tc = 'i';
            break;
        case TT_STYLE_UNDERLINE:
            tc = 'i';
            break;
    }

    if (ev->style_value_bool == false && first == true) {
        /* Don't start the line with a </b> for example */
        return;
    }
    fprintf(f, "<%s%c>", ev->style_value_bool ? "" : "/", tc);
}

static void render_tagtext_events(const struct tagtext_event *events, const struct subobj_caption_region *region, int furi_len, const struct chars_for_furi_result *furis, FILE *f)
{
    struct tagtext_event current_styles[TT_STYLE_COUNT_];
    int chr_count = 0;

    if (region->ref->is_ruby)
        return;

    for (const struct tagtext_event *event = events; event < arrendptr(events); event++) {
        bool was_char = false;
        if (event->type == TT_EVENT_TYPE_STYLE) {
            if (opt_srt_tags) {
                tagtext_update_current_styles(event, current_styles);
                render_tagtext_event_style(event, !was_char, f);
            }
        } else if (event->type == TT_EVENT_TYPE_CHAR) {
            was_char = true;
            fputs(event->ref_so_chr->ref->u8str, f);

            for (int fi = 0; fi < furi_len; fi++) {
                if (furis[fi].chars_region == region->ref && furis[fi].chars_span_to == chr_count) {
                    fputs("(", f);
                    for (uint32_t c = 0; c < furis[fi].furi_region->char_count; c++) {
                        fputs(furis[fi].furi_region->chars[c].u8str, f);
                    }
                    fputs(")", f);
                }
            }

            chr_count++;
        }
    }

    if (opt_srt_tags) {
        /* Close all still open tags */
        for (int i = 0; i < TT_STYLE_COUNT_; i++) {
            switch (current_styles[i].style) {
                case TT_STYLE_BOLD:
                case TT_STYLE_ITALIC:
                case TT_STYLE_UNDERLINE:
                    if (current_styles[i].style_value_bool == true) {
                        current_styles[i].style_value_bool = false;
                        render_tagtext_event_style(&current_styles[i], true, f);
                    }
                    break;
                default:
                    break;
            }
        }
        fputs("</font>", f);
    }
    fputs("\n", f);
}

static void render_caption(struct srt_ctx *sc, struct tagtext_caption *caption, FILE *f)
{
    if (arrlen(caption->tagtexts) <= 0)
        return;

    render_line_header(sc, caption, f);

    struct chars_for_furi_result rubys[MAX_FURI_REGIONS];
    int rubylen = 0;
    if (opt_srt_furi) {
        rubylen = util_find_chars_for_furi(caption->ref_subobj, rubys);
    }

    for (intptr_t tti = 0; tti < arrlen(caption->tagtexts); tti++) {
        struct tagtext *tt = &caption->tagtexts[tti];
        const struct subobj_caption_region *region = &caption->ref_subobj->so_caption.so_regions[tti];

        render_tagtext_events(tt->events, region, rubylen, rubys, f);
    }
    fputs("\n", f);
}

enum error srt_write(const struct subobj_ctx *sctx, const pchar *filepath)
{
    struct srt_ctx sc = { 0 };
    FILE *f = pfopen(filepath, PSTR("wb"));
    if (f == NULL) {
        return -errno;
    }

    struct tagtext_caption *tt_captions = NULL;
    enum error err = tagtext_parse_captions(sctx->subobjs, &tt_captions, NULL);
    if (err != NOERR) {
        fclose(f);
        return err;
    }

    for (intptr_t i = 0; i < arrlen(tt_captions); i++) {
        struct tagtext_caption *ttc = &tt_captions[i];

        render_caption(&sc, ttc, f);
    }

    tagtext_captions_free(tt_captions);
    fclose(f);
    return NOERR;
}
