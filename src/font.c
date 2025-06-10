#include "font.h"

#undef FTERRORS_H_
/* Define error freetype error descriptions */
#define FT_ERROR_START_LIST const pchar *ft_error_strings[] = {
#define FT_ERRORDEF( e, v, s )  [v] = PSTR(s),
#define FT_ERROR_END_LIST };
#include <freetype/fterrors.h>

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <uchar.h>

#include "util.h"
#include "log.h"

#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H

static char *get_font_sfnt_name(FT_Face face);

FT_Library ftlib = NULL;

void font_init()
{
    assert(ftlib == NULL);

    int err = FT_Init_FreeType(&ftlib);
    assert(err == FT_Err_Ok);
}

void font_dinit()
{
    assert(ftlib != NULL);

    FT_Done_FreeType(ftlib);
    ftlib = NULL;
}

static FT_Error open_font_file(struct font *ft, FT_Library ftlib, const pchar *fontpath, FT_Long face_index)
{
    FT_Open_Args oargs;

#if defined(_WIN32)
    int n = platform_memory_map_file(fontpath, &ft->font_file_memmap);
    if (n != 0) {
        return FT_Err_Cannot_Open_Resource;
    }
    oargs.flags = FT_OPEN_MEMORY;
    oargs.memory_base = ft->font_file_memmap.addr;
    oargs.memory_size = ft->font_file_memmap.size;
#elif defined(__linux__)
    oargs.flags = FT_OPEN_PATHNAME;
    oargs.pathname = (char*)fontpath;
#endif

    return FT_Open_Face(ftlib, &oargs, face_index, &ft->face);
}

static enum error font_create_with_index(const pchar *fontpath, const pchar *fontface, struct font *out_ft)
{
    struct font ft = { 0 };
    FT_Error err;
    FT_Long i, num_faces, face_idx = -1;
    char *fend;
#ifdef _WIN32
    char u8_fontface_target_name[1024];
    platform_pchar_to_u8(fontface, -1, u8_fontface_target_name, sizeof(u8_fontface_target_name));
#else
    const char *u8_fontface_target_name = fontface;
#endif

    errno = 0;
    face_idx = strtol(u8_fontface_target_name, &fend, 10);
    if (errno != 0 || *fend != '\0')
        face_idx = -1;

    err = open_font_file(&ft, ftlib, fontpath, face_idx);
    if (err != FT_Err_Ok) {
        log_error("Failed to load font at '%s' (%s): %s\n", fontpath, fontface, ft_error_strings[err]);
        font_destroy(&ft);
        return ERR_FREETYPE;
    }
    if (face_idx != -1) {
        /* Loaded by index */
        ft.fontname = get_font_sfnt_name(ft.face);
        *out_ft = ft;
        return NOERR;
    }

    num_faces = ft.face->num_faces;
    font_destroy(&ft);

    for (i = 0; i < num_faces; i++) {
        /* Loaded by name */
        char *name;

        err = open_font_file(&ft, ftlib, fontpath, i);
        if (err != FT_Err_Ok) {
            log_error("Failed to load font at '%s' (%ld): %s\n", fontpath, i, ft_error_strings[err]);
            font_destroy(&ft);
            return ERR_FREETYPE;
        }
        name = get_font_sfnt_name(ft.face);
        if (strcmp(name, u8_fontface_target_name) == 0) {
            ft.fontname = name;
            *out_ft = ft;
            return NOERR;
        }
        free(name);
        font_destroy(&ft);
    }

    return ERR_FONT_FACE_NOT_FOUND;
}

enum error font_create(const pchar *fontpath, const pchar *fontface, struct font *out_ft)
{
    FT_Error err;
    assert(ftlib != NULL);

    if (fontface != NULL) {
        return font_create_with_index(fontpath, fontface, out_ft);
    }

    /* Use the default face index */
    err = open_font_file(out_ft, ftlib, fontpath, 0);
    if (err != FT_Err_Ok) {
        log_error("Failed to load font at '%s': %s\n", fontpath, ft_error_strings[err]);
        return ERR_FREETYPE;
    }
    out_ft->fontname = get_font_sfnt_name(out_ft->face);
    return NOERR;
}

void font_destroy(struct font *ft)
{
    if (ft->face)
		FT_Done_Face(ft->face);
    if (ft->fontname)
		free(ft->fontname);
#ifdef _WIN32
    platform_memory_unmap_file(&ft->font_file_memmap);
#endif
    memset(ft, 0, sizeof(*ft));
}

static const char *font_get_ps_name(FT_Face face)
{
    return FT_Get_Postscript_Name(face);
}

static void font_ucs_to_u8(char16_t *in_ucs, size_t in_ucs_bcount, char *out_u8, size_t out_u8_bsize)
{
    /* I will fix this when I find a font that breaks it */
    assert(out_u8_bsize > in_ucs_bcount / sizeof(char16_t));
    int i;
    for (i = 0; i < in_ucs_bcount / sizeof(*in_ucs); i++) {
        assert((in_ucs[i] & 0xFF) == 0);

        out_u8[i] = (char)((in_ucs[i] >> 8) & 0xFF);
    }
    out_u8[i] = '\0';
}

static void decode_sfnt_string_to_u8(const FT_SfntName *sf, size_t bsize, char *out)
{
    if (sf->platform_id == TT_PLATFORM_MACINTOSH && sf->encoding_id == TT_MAC_ID_ROMAN) {
        /* Is ascii (or utf8?) */
        assert(bsize > sf->string_len);
        memcpy(out, sf->string, sf->string_len);
        out[sf->string_len] = '\0';
        return;
    }
    assert(sf->platform_id == TT_PLATFORM_MICROSOFT);
    assert(sf->encoding_id == TT_MS_ID_UNICODE_CS);

    font_ucs_to_u8((char16_t*)sf->string, sf->string_len, out, bsize);
}

static char *get_font_sfnt_name(FT_Face face)
{
    char buf[1024];
    FT_SfntName sfnt;

    for (int i = 0; i < FT_Get_Sfnt_Name_Count(face); i++) {
        FT_Error err = FT_Get_Sfnt_Name(face, i, &sfnt);
        assert(err == FT_Err_Ok);

        if (sfnt.name_id != TT_NAME_ID_FONT_FAMILY)
            continue;

        decode_sfnt_string_to_u8(&sfnt, sizeof(buf), buf);
        return strdup(buf);
    }

    return strdup(font_get_ps_name(face));
}
