#ifndef ARIB2ASS_DRCS_H
#define ARIB2ASS_DRCS_H
#include <aribcaption/aribcaption.h>
#include <stdint.h>
#include <stdbool.h>

#include "subobj.h"
#include "error.h"


enum error drcs_dump(const struct subobj_ctx *s);
void drcs_replace(aribcc_drcsmap_t *drcs_map, aribcc_caption_char_t *chr);

#endif /* ARIB2ASS_DRCS_H */
