#ifndef ARIB2ASS_DRCS_H
#define ARIB2ASS_DRCS_H
#include <aribcaption/aribcaption.h>
#include <stdint.h>
#include <stdbool.h>
#include <toml.h>

#include "subobj.h"
#include "error.h"

/*
 * Free dynamic replacement table
 */
void drcs_free();

enum error drcs_dump(const struct subobj_ctx *s);
void drcs_replace(aribcc_drcsmap_t *drcs_map, aribcc_caption_char_t *chr);

/* If md5 is NULL, add it as the default replacement char */
enum error drcs_add_mapping(const char *md5, char32_t codepoint);
enum error drcs_add_mapping_u8(const char *md5, const char *u8char);
void drcs_add_mapping_from_file(const pchar *path);

/* This will not handle the 'files' key.
 * that is only valid from the config file
 * */
void drcs_add_mapping_from_table(const toml_table_t *tbl);

#endif /* ARIB2ASS_DRCS_H */
