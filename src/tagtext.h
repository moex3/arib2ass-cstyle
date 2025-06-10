#ifndef ARIB2ASS_TAGTEXT_H
#define ARIB2ASS_TAGTEXT_H
#include <aribcaption/aribcaption.h>

#include "error.h"
#include "subobj.h"
#include "defs.h"


enum tagtext_style {
    TT_STYLE_TEXT_COLOR = 0, /* Main text color */
    TT_STYLE_BACK_COLOR,     /* ??? */
    TT_STYLE_STROKE_COLOR,   /* Outline color */
    TT_STYLE_SCALE_X,
    TT_STYLE_SCALE_Y,
    TT_STYLE_SPACING_X,
    TT_STYLE_SPACING_Y,
    TT_STYLE_CHAR_HEIGHT,
    TT_STYLE_CHAR_WIDTH,

    TT_STYLE_BOLD,
    TT_STYLE_ITALIC,
    TT_STYLE_UNDERLINE,
    TT_STYLE_STROKE,

    //TT_STYLE_POS_XY,

    TT_STYLE_COUNT_,

    /* This looks pretty wrong, but it kind of makes sense to put
     * it here, as it is not really a real style, just a control style
     */
    TT_STYLE_RESET,
};


/* A stream of tags and text. Handles all inline style changes in the
 * same way .ass does. That is, you have style changes, that
 * apply to all of the text after it.
 * The first events are always event types, then a character of text, then
 * only style events that are changed for the next character (if any) */
struct tagtext {
    /* An event is either a string of text, or a style-change */
    struct tagtext_event {
        enum tagtext_event_type {
            TT_EVENT_TYPE_CHAR,
            TT_EVENT_TYPE_STYLE,
        } type;

        /* If type is style, this style is changed to style_value */
        enum tagtext_style style;

        union {
            /* if text type. A reference pointer to the modified subobj caption struct */
            const struct subobj_caption_char *ref_so_chr;
            /* The new value for style */
            union {
                uintptr_t style_value;
                float style_value_float;
                uint32_t style_value_u32;
                bool style_value_bool;
            };
        };
    } stb_array *events;
};

struct tagtext_caption {
    /* stb_ds array with element count of ref_subobj->caption.region_count */
    struct tagtext stb_array *tagtexts;
    /* a reference to the subobj that it was parsed from */
    const struct subobj *ref_subobj;
};

enum error tagtext_parse_captions(const struct subobj *subobjs,
        struct tagtext_caption **out_tt_captions, struct tagtext_event out_default_styles[TT_STYLE_COUNT_]);
void tagtext_captions_free(struct tagtext_caption *tt_captions);

void tagtext_update_current_styles(const struct tagtext_event *event, struct tagtext_event out_styles[TT_STYLE_COUNT_]);
struct tagtext_event *tagtext_search_style_for_char(const struct tagtext_event *events, intptr_t event_idx, enum tagtext_style style);

#endif /* ARIB2ASS_TAGTEXT_H */
