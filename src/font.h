#ifndef _VTT2ASS_FONT_H
#define _VTT2ASS_FONT_H
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include "error.h"
#include "platform.h"

void font_init();
void font_dinit();

struct font {
    FT_Face face;
    char *fontname; /* utf8 */

#ifdef _WIN32
    struct memory_file_map font_file_memmap;
#endif
};

enum error font_create(const pchar *fontpath, const pchar *fontface, struct font *out_ft);
void font_destroy(struct font *ft);

#endif /* _VTT2ASS_FONT_H */
