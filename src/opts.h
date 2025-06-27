#ifndef A2AC_OPTS_H
#define A2AC_OPTS_H
#include <stdbool.h>
#include "platform.h"
#include "error.h"
#include "defs.h"

extern enum log_level opt_log_level;
extern bool opt_dump_drcs;
enum dump_drcs_format {
    BIN, PNG,
};
extern enum dump_drcs_format opt_dump_drcs_format;

extern bool opt_ass_do;
extern const pchar *opt_ass_font_path;

/* Can be queried with fc-query */
extern const pchar *opt_ass_font_face;
extern bool opt_ass_fs_adjust;
extern bool opt_ass_optimize;
extern bool opt_ass_force_bold;
extern bool opt_ass_force_border;
extern bool opt_ass_merge_regions;
extern bool opt_ass_debug_boxes;
// case: MS PGothic く
extern bool opt_ass_center_spacing;
/* -1 for no, other value for that value */
extern int opt_ass_constant_spacing;
/* Only makes sense when opt_ass_constant_spacing is 1
 * If this is true, then shift ruby region positions
 * so it will still line up correctly */
extern bool opt_ass_shift_ruby;
extern const bool opt_ass_only_furi;


extern bool opt_srt_do;
extern bool opt_srt_tags;
extern bool opt_srt_furi;

extern const pchar stb_array **opt_input_files;

extern pchar *opt_ass_output;
extern bool opt_ass_output_dir;
extern pchar *opt_srt_output;
extern bool opt_srt_output_dir;

/*
 * Parse command line arguments
 */
enum error opts_parse_cmdline(int argc, pchar **argv);

/* Free any resources allocated by config */
void opts_free();

#endif /* A2AC_OPTS_H */
