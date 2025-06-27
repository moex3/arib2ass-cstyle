#ifndef ARIB2ASS_PNG_H
#define ARIB2ASS_PNG_H
#include "error.h"
#include <stdint.h>

enum error png_encode(int w, int h, int depth, uint8_t *px, size_t pxsize,
        uint8_t **out_buf, size_t *out_buf_size);

#endif /* ARIB2ASS_PNG_H */

