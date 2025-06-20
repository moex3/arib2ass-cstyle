#include "util.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include "log.h"

void unicode_to_utf8(char32_t cp, char u8[8])
{
#ifdef __linux__
    mbstate_t s = {0};
    size_t r = wcrtomb(u8, cp, &s);
    assert(r != (size_t)-1);
    assert(r < 8);
    u8[r] = '\0';
#else

    /* https://en.wikipedia.org/wiki/UTF-8#Description */
    if (cp <= 0x7F) {
        u8[0] = (char)cp;
        u8[1] = '\0';
    } else if (cp <= 0x7FF) {
        u8[0] = 0xC0 | (cp >> 6);
        u8[1] = 0x80 | (cp & 0x3F);
        u8[2] = '\0';
    } else if (cp <= 0xFFFF) {
        u8[0] = 0xE0 | (cp >> 12);
        u8[1] = 0x80 | ((cp >> 6) & 0x3F);
        u8[2] = 0x80 | (cp & 0x3F);
        u8[3] = '\0';
    } else if (cp <= 0x10FFFF) {
        u8[0] = 0xF0 | (cp >> 18);
        u8[1] = 0x80 | ((cp >> 12) & 0x3F);
        u8[2] = 0x80 | ((cp >> 6) & 0x3F);
        u8[3] = 0x80 | (cp & 0x3F);
        u8[4] = '\0';
    } else {
        u8[0] = '\0';
        assert(false);
    }
#endif
}

char32_t utf8_to_unicode(const char *u8)
{
    char32_t cp;
#ifdef __linux__
    mbstate_t s = {0};
    static_assert(sizeof(char32_t) == sizeof(wchar_t));
    size_t r = mbrtowc((wchar_t*)&cp, u8, MB_CUR_MAX, &s);
    if (r == (size_t)-1 || r == (size_t)-2)
        goto err;

#else

    // https://en.wikipedia.org/wiki/UTF-8
    // Passes the tests from https://stackoverflow.com/questions/1301402/example-invalid-utf8-string
    // except for codepoints outside of unicode, but w/e
    cp = 0;
    if ((unsigned char)u8[0] <= 0x7f)
        return (char32_t)u8[0];

    if ((u8[0] & 0xE0) == 0xC0) {
        if ((u8[1] & 0xC0) != 0x80) goto err;
        cp = ((u8[0] & 0x1F) << 6) | (u8[1] & 0x3F);
    } else if ((u8[0] & 0xF0) == 0xE0) {
        if ((u8[1] & 0xC0) != 0x80 || (u8[2] & 0xC0) != 0x80) goto err;
        cp = ((u8[0] & 0x0F) << 12) | ((u8[1] & 0x3F) << 6) | (u8[2] & 0x3F);
    } else if ((u8[0] & 0xF8) == 0xF0) {
        if ((u8[1] & 0xC0) != 0x80 || (u8[2] & 0xC0) != 0x80 || (u8[3] & 0xC0) != 0x80) goto err;
        cp = ((u8[0] & 0x07) << 21) | ((u8[1] & 0x3F) << 12) | ((u8[2] & 0x3F) << 6) | (u8[3] & 0x3F);
    } else {
        goto err;
    }

#endif

    return cp;
err:
    return 0;
}

void arrmovelem(void *arr, intptr_t elem_idx, intptr_t to_idx, size_t elem_size)
{
    char tmp[128];
    assert(sizeof(tmp) > elem_size);

    char *to_ptr = (char*)arr + (to_idx * elem_size);
    char *elem_ptr = (char*)arr + (elem_idx * elem_size);
    memcpy(tmp, elem_ptr, elem_size);

    if (elem_idx > to_idx) {
        size_t movsize = (elem_idx - to_idx) * elem_size;
        memmove(to_ptr + elem_size, to_ptr, movsize);
    } else {
        size_t movsize = (to_idx - elem_idx) * elem_size;
        memmove(elem_ptr, elem_ptr + elem_size, movsize);
    }

    memcpy(to_ptr, tmp, elem_size);
}

const char *sftext_format(struct sftext *sf, float f)
{
    int w;
    char *start, *end;

    start = &sf->buf[sf->idx];
    w = snprintf(start, sizeof(sf->buf) - sf->idx, "%.02f", f);
    sf->idx += w;
    assert(sf->idx < sizeof(sf->buf));

    end = &sf->buf[sf->idx - 1];
    while (*end == '0') {
        end--;
        sf->idx--;
    }
    if (*end != '.') {
        end++;
        sf->idx++;
    }
    *end = '\0';
    return start;
}

struct rect {
    int x, y, w, h;
};
static bool is_region_for_ruby(const struct rect *region, const struct rect *ruby)
{
    if (!((region->y == ruby->y + ruby->h)
            /*|| (region->y + region->height == ruby->y)*/)) {
        return false;
    }

    if (!(region->x <= ruby->x && region->x + region->w >= ruby->x + ruby->w))
        return false;

    return true;
}

const struct subobj_caption_region *util_main_so_region_for_ruby(const struct subobj_caption *caption, const struct subobj_caption_region *ruby)
{
    assert(ruby->ref->is_ruby);
    struct rect r_region, r_ruby;

    r_ruby = (struct rect){
        .x = ruby->x,     .y = ruby->y,
        .w = ruby->width, .h = ruby->height,
    };
    for (struct subobj_caption_region *region = caption->so_regions; region < arrendptr(caption->so_regions); region++) {

        if (region->ref->is_ruby == true)
            continue;

        if (arrlen(caption->so_regions) == 2) {
            /* If only 2 region, then the non-ruby one must be the main one */
            return region;
        }

        r_region = (struct rect){
            .x = region->x,     .y = region->y,
            .w = region->width, .h = region->height,
        };
        if (is_region_for_ruby(&r_region, &r_ruby) == true)
            return region;

    }
    return NULL;
}

const aribcc_caption_region_t *util_main_region_for_ruby(const aribcc_caption_t *caption, const aribcc_caption_region_t *ruby)
{
    assert(ruby->is_ruby);
    struct rect r_region, r_ruby;

    r_ruby = (struct rect){
        .x = ruby->x,     .y = ruby->y,
        .w = ruby->width, .h = ruby->height,
    };
    for (int i = 0; i < caption->region_count; i++) {
        const aribcc_caption_region_t *region = &caption->regions[i];

        if (region->is_ruby == true)
            continue;

        if (caption->region_count == 2) {
            /* If only 2 region, then the non-ruby one must be the main one */
            return region;
        }

        r_region = (struct rect){
            .x = region->x,     .y = region->y,
            .w = region->width, .h = region->height,
        };
        if (is_region_for_ruby(&r_region, &r_ruby) == true)
            return region;

    }
    return NULL;
}

#if 0
int util_find_chars_for_furi(const struct subobj_caption *so_caption, struct chars_for_furi_result out_res[MAX_FURI_REGIONS])
{
    int out_idx = 0, ruby_count = 0;

    for (int i = 0; i < arrlen(so_caption->so_regions); i++) {
        const struct subobj_caption_region *ruby, *region;
        int span_start = -1, span_end = -1;

        ruby = &so_caption->so_regions[i];
        if (ruby->ref->is_ruby == false)
            continue;
        ruby_count++;

        region = util_main_so_region_for_ruby(so_caption, ruby);
        assert(region);
        for (int c = 0; c < arrlen(region->so_chars); c++) {
            if (region->so_chars[c].ref->x == ruby->x) {
                span_start = c;
            }
            if (ruby->x + ruby->width <= region->so_chars[c].ref->x || c == arrlen(region->so_chars) - 1) {
                if (ruby->x + ruby->width <= region->so_chars[c].ref->x)
                    span_end = c - 1;
                else
                    span_end = c;

                out_res[out_idx++] = (struct chars_for_furi_result){
                    .furi_region = ruby,
                    .chars_region = region,
                    .chars_span_from = span_start,
                    .chars_span_to = span_end,
                };
                assert(out_idx <= MAX_FURI_REGIONS);
                break;
            }
        }
    }

    assert(ruby_count == out_idx);
    return out_idx;
}
#else
int util_find_chars_for_furi(const struct subobj *so, struct chars_for_furi_result out_res[MAX_FURI_REGIONS])
{
    int out_idx = 0, ruby_count = 0;

    for (int i = 0; i < so->caption_ref.region_count; i++) {
        const aribcc_caption_region_t *ruby, *region;
        int span_start = -1, span_end = -1;

        ruby = &so->caption_ref.regions[i]; 
        if (ruby->is_ruby == false)
            continue;
        ruby_count++;

        region = util_main_region_for_ruby(&so->caption_ref, ruby);
        assert(region);
        for (int c = 0; c < region->char_count; c++) {
            const aribcc_caption_char_t *chr = &region->chars[c];

            int cw = (chr->char_width + chr->char_horizontal_spacing) * chr->char_horizontal_scale;
            if (ruby->x >= chr->x && ruby->x < chr->x + cw) {
                span_start = c;
            }
            if (ruby->x + ruby->width > chr->x && ruby->x + ruby->width <= chr->x + cw) {
                span_end = c;
            }
        }
        if (span_end == -1) {
            if (ruby->x + ruby->width > region->x + region->width)
                span_end = region->char_count - 1;
        }

        if (span_start != -1 && span_end != -1) {
            out_res[out_idx++] = (struct chars_for_furi_result){
                .furi_region = ruby,
                .chars_region = region,
                .chars_span_from = span_start,
                .chars_span_to = span_end,
            };
            assert(out_idx <= MAX_FURI_REGIONS);
        }
    }

    if (ruby_count != out_idx) {
        log_warning("Not found chars for all ruby\n");
        //assert(ruby_count == out_idx);
    }
    return out_idx;
}
#endif

void util_ms_to_htime(int64_t tms, pchar out[32])
{
    int64_t h, m, s, ms;
    h = tms / H_IN_MS;
    tms %= H_IN_MS;

    m = tms / M_IN_MS;
    tms %= M_IN_MS;

    s = tms / S_IN_MS;
    ms = tms % S_IN_MS;

    psnprintf(out, 32, PSTR("%02") PSTR2(PRId64) PSTR(":%02") PSTR2(PRId64)
            PSTR(":%02") PSTR2(PRId64) PSTR(".%03") PSTR2(PRId64), h, m, s, ms);
}

pchar get_str_last_char(const pchar *str)
{
    if (str == NULL || str[0] == PSTR('\0'))
        return PSTR('\0');
    return str[pstrlen(str) - 1];
}
