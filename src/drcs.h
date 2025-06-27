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
/* Default write it to ./drcs/md5hash.png */
enum error drcs_write_to_png(aribcc_drcs_t *drcs);

/*
 * Get a unicode codepoint for drcs replacement.
 * A return value of 0 means the replacement was not found.
 */
char32_t drcs_get_replacement_ucs4_by_md5(const char *md5);

/* If md5 is NULL, add it as the default replacement char */
enum error drcs_add_mapping(const char *md5, char32_t codepoint);
enum error drcs_add_mapping_u8(const char *md5, const char *u8char);
void drcs_add_mapping_from_file(const pchar *path);

/* This will not handle the 'files' key.
 * that is only valid from the config file
 * */
void drcs_add_mapping_from_table(const toml_table_t *tbl);

#endif /* ARIB2ASS_DRCS_H */
