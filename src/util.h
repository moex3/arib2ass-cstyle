#ifndef ARIB2ASS_UTIL_H
#define ARIB2ASS_UTIL_H
#include <stddef.h>
#include <uchar.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <aribcaption/aribcaption.h>
#include "subobj.h"
#include "platform.h"

#ifdef __linux__
#define dbg asm("int $3")
#else
#define dbg
#endif

#define ARRAY_COUNT(a) (sizeof(a) / sizeof(a[0]))
#define S_IN_MS (1000)
#define M_IN_MS (S_IN_MS * 60)
#define H_IN_MS (M_IN_MS * 60)
#define nnfree(x) if (x != NULL) free((void*)x)
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define arrendptr(ds_arr) (&ds_arr[arrlen(ds_arr)])

#define MEASURE_START(id) struct ptimespec __a_ ## id; PLATFORM_CURRENT_TIMESPEC(&__a_ ## id)
#define MEASURE_END(id, outms) struct ptimespec __b_ ## id; \
    PLATFORM_CURRENT_TIMESPEC(&__b_ ## id); \
    outms = (__b_ ## id.tv_sec - __a_ ## id.tv_sec) * 1000 + ((__b_ ## id.tv_nsec - __a_ ## id.tv_nsec) / 1000000)

/* Returns the byte length of the utf8 character in 'chr'
 * Only pass well-formed utf8 characters here
 */
void   unicode_to_utf8(char32_t cp, char u8[8]);

void arrmovelem(void *arr, intptr_t elem_idx, intptr_t to_idx, size_t elem_size);

/* Short Float Text Format
 * Format a float value into text by 
 * only giving at most 2 decimal places of
 * precision, and stripping extra 0s */
struct sftext {
    size_t idx;
    char buf[4096];
};
const char *sftext_format(struct sftext *sf, float f);
#define sftf_init() struct sftext __sft; __sft.idx = 0
#define sftf(f) sftext_format(&__sft, f)

const struct subobj_caption_region *util_main_so_region_for_ruby(const struct subobj_caption *caption, const struct subobj_caption_region *ruby);
const aribcc_caption_region_t *util_main_region_for_ruby(const aribcc_caption_t *caption, const aribcc_caption_region_t *ruby);

/* Find the corresponding chars for all furigana text in a caption
 * Return results in a chars_for_furi_result array,
 * with references pointing to the given caption in the arguments
 * Return the number of out results array filled in */
#define MAX_FURI_REGIONS 16
struct chars_for_furi_result {
    const aribcc_caption_region_t *furi_region, *chars_region;
    int chars_span_from, chars_span_to;
};
int util_find_chars_for_furi(const struct subobj *so, struct chars_for_furi_result out_res[MAX_FURI_REGIONS]);

void util_ms_to_htime(int64_t tms, pchar out[32]);

#endif /* ARIB2ASS_UTIL_H */
