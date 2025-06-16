#include "opts.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>
#include <toml.h>
#include <getopt.h>

#include "stb_ds.h"
#include "util.h"
#include "log.h"
#include "version.h"



#define B(arg)  ((arg) ? PSTR("true") : PSTR("false"))
#define B8(arg) ((arg) ?     ("true") :     ("false"))

static pchar *opt_output = NULL;
static pchar *opt_dump_config = NULL;
enum log_level opt_log_level = LOG_MSG;
bool opt_dump_drcs = false;

bool opt_ass_do = false;
const pchar *opt_ass_font_path = NULL;
const pchar *opt_ass_font_face = NULL;
bool opt_ass_fs_adjust = false;
bool opt_ass_optimize = true;
bool opt_ass_force_bold = false;
bool opt_ass_force_border = false;
bool opt_ass_merge_regions = false;
bool opt_ass_debug_boxes = false;
// case: MS PGothic く
bool opt_ass_center_spacing = true;
/* -1 for no, other value for that value */
int opt_ass_constant_spacing = -1;
/* Only makes sense when opt_ass_constant_spacing is 1
 * If this is true, then shift ruby region positions
 * so it will still line up correctly */
bool opt_ass_shift_ruby = false;
const bool opt_ass_only_furi = false;

bool opt_srt_do = false;
bool opt_srt_tags = false;
bool opt_srt_furi = false;

const pchar stb_array **opt_input_files = NULL;
pchar *opt_ass_output = NULL;
bool opt_ass_output_dir = false;
pchar *opt_srt_output = NULL;
bool opt_srt_output_dir = false;

enum short_opts {
    SOPT_HELP = 'h',
    SOPT_CONFIG_FILE = 'c',
    SOPT_DUMP_CONFIG = 0x100,
    SOPT_OUTPUT = 'o',
    SOPT_VERBOSE = 'v',
    SOPT_QUIET = 'q',
    SOPT_DUMP_DRCS = 0x102,
    SOPT_ASS_NO_OPTIMIZE = 'Z',
    SOPT_ASS_FORCE_BOLD = 'b',
    SOPT_ASS_FORCE_BORDER = 'B',
    SOPT_ASS_MERGE_REGIONS = 'm',
    SOPT_ASS_DEBUG_BOXES = 'd',
    SOPT_ASS_NO_CENTER_SPACING = 'C',
    SOPT_ASS_CONSTANT_SPACING = 's',
    SOPT_ASS_SHIFT_RUBY = 'r',
    SOPT_ASS_FONT_PATH = 'f',
    SOPT_ASS_FONT_FACE = 0x101,
    SOPT_ASS_FS_ADJUST = 'a',

    SOPT_SRT_TAGS = 't',
    SOPT_SRT_FURI = 'f',
};

/* '+' to stop processing args at the first non-opt argument */
static const pchar arg_string_pre[] = PSTR("+c:o:hqv");
static const struct option arg_options_pre[] = {
    { PSTR("config-file"), required_argument, NULL, SOPT_CONFIG_FILE },
    { PSTR("dump-config"), required_argument, NULL, SOPT_DUMP_CONFIG },
    { PSTR("help"),        no_argument,       NULL, SOPT_HELP },
    { PSTR("output"),      required_argument, NULL, SOPT_OUTPUT },
    { PSTR("verbose"),     no_argument,       NULL, SOPT_VERBOSE },
    { PSTR("quiet"),       no_argument,       NULL, SOPT_QUIET },
    { PSTR("dump-drcs"),   no_argument,       NULL, SOPT_DUMP_DRCS },
    { 0 },
};

static const pchar arg_string_ass[] = PSTR("+ho:ZbBmdCs:rf:a");
static const struct option arg_options_ass[] = {
    { PSTR("help"),               no_argument,       NULL, SOPT_HELP },
    { PSTR("output"),             required_argument, NULL, SOPT_OUTPUT },
    { PSTR("no-optimize"),        no_argument,       NULL, SOPT_ASS_NO_OPTIMIZE },
    { PSTR("force-bold"),         no_argument,       NULL, SOPT_ASS_FORCE_BOLD },
    { PSTR("force-border"),       no_argument,       NULL, SOPT_ASS_FORCE_BORDER },
    { PSTR("merge-regions"),      no_argument,       NULL, SOPT_ASS_MERGE_REGIONS },
    { PSTR("debug-boxes"),        no_argument,       NULL, SOPT_ASS_DEBUG_BOXES },
    { PSTR("no-center-spacing"),  no_argument,       NULL, SOPT_ASS_NO_CENTER_SPACING },
    { PSTR("constant-spacing"),   no_argument,       NULL, SOPT_ASS_CONSTANT_SPACING },
    { PSTR("shift-ruby"),         no_argument,       NULL, SOPT_ASS_SHIFT_RUBY },
    { PSTR("font"),               required_argument, NULL, SOPT_ASS_FONT_PATH },
    { PSTR("font-face"),          required_argument, NULL, SOPT_ASS_FONT_FACE },
    { PSTR("fs-adjust"),          no_argument,       NULL, SOPT_ASS_FS_ADJUST },
};

static const pchar arg_string_srt[] = PSTR("+ho:tf");
static const struct option arg_options_srt[] = {
    { PSTR("help"),   no_argument,       NULL, SOPT_HELP },
    { PSTR("output"), required_argument, NULL, SOPT_OUTPUT },
    { PSTR("tags"),   no_argument,       NULL, SOPT_SRT_TAGS },
    { PSTR("furi"),   no_argument,       NULL, SOPT_SRT_FURI },
};


static void print_help()
{
    pprintf(
            PSTR("arib2ass-cstyle")
#ifdef A2AC_VERSION
            PSTR(" (") PSTR2(A2AC_VERSION) PSTR(")")
#endif
            PSTR("\n")
            PSTR("Usage: ./a2ac [global-opts] ass [opts-for-ass] srt [opts-for-srt] input .ts files ...\n")
            PSTR("\n")
            PSTR("GLOBAL OPTIONS\n")
            PSTR("  -h   --help               Show this help text\n")
            PSTR("  -c   --config-file        Read options from this config file\n")
            PSTR("       --dump-config        Write the currently selected options to a file\n")
            PSTR("  -o   --output             Output for all outputs (should be a directory)\n")
            PSTR("  -v   --verbose            Verbose output\n")
            PSTR("  -q   --quiet              Quiet output\n")
            PSTR("       --dump-drcs          Write all drcs pixel data found to ./drcs/\n")
            PSTR("\n")
            PSTR("ASS OPTIONS:\n")
            PSTR("  -o   --output             Output resulting file into this path, or directory\n")
            PSTR("  -f   --font               The path to the font to use (%s)\n")
            PSTR("       --font-face          The font name (or index) to use if font is a .ttc file (%s)\n")
            PSTR("  -Z   --no-optimize        Do not optimize line styles (%s)\n")
            PSTR("  -b   --force-bold         Always use bold (%s)\n")
            PSTR("  -B   --force-border       Always use a border (%s)\n")
            PSTR("  -m   --merge-regions      Combine 2 left-aligned, directly connected lines into one (%s)\n")
            PSTR("  -d   --debug-boxes        Include debug boxes in the resulting file (%s)\n")
            PSTR("  -C   --no-center-spacing  Do not align each character in the middle of its given bounding box (%s)\n")
            PSTR("  -s   --constant-spacing   Use this many pixels between each character. (%d)\n")
            PSTR("  -r   --shift-ruby         When using constant-spacing, try to find and shift the furigana to its correct spot (%s)\n")
            PSTR("  -a   --fs-adjust          Adjust smaller fonts to make them appear with the expected size (%s)\n")
            PSTR("\n")
            PSTR("SRT OPTIONS:\n")
            PSTR("  -o   --output             Output resulting file into this path, or directory\n")
            PSTR("  -t   --tags               Write formatting tags (%s)\n")
            PSTR("  -f   --furi               Try to write furigana in parenthesis (%s)\n")
            PSTR("\n"),
            opt_ass_font_path, opt_ass_font_face, B(!opt_ass_optimize), B(opt_ass_force_bold), B(opt_ass_force_border),
            B(opt_ass_merge_regions), B(opt_ass_debug_boxes), B(!opt_ass_center_spacing), opt_ass_constant_spacing,
            B(opt_ass_shift_ruby), B(opt_ass_fs_adjust), B(opt_srt_tags), B(opt_srt_furi)
            );
}

static char *toml_escape_string(const char *in, char *out, size_t out_size)
{
    int oi = 0;
    for (int i = 0; in[i] != '\0'; i++) {
        char c = in[i];

		// TODO: handle more escapes
        if (c == '\\') {
            out[oi++] = '\\';
            assert(oi < out_size);
        }

        out[oi++] = c;
		assert(oi < out_size);
    }
    out[oi] = '\0';
    return out;
}

static enum error dump_config(const pchar *outfile)
{
    char escaped_buffer[1024];
#define TESC(u8str) (toml_escape_string(u8str, escaped_buffer, sizeof(escaped_buffer)))

    FILE *f = pfopen(outfile, PSTR("wb"));
    if (f == NULL) {
        enum error err = -errno;
        log_error("Failed to open config dump file: '%s': %s\n", outfile, error_to_string(err));
        return err;
    }

    if (opt_output)
        fprintf(f, "output = \"%s\"\n", TESC(PCu8(opt_output)));

    if (opt_log_level == LOG_DEBUG)
        fprintf(f, "logging = %s\n", "\"verbose\"");
    else if (opt_log_level == LOG_ERROR)
        fprintf(f, "logging = %s\n", "\"quiet\"");

    fprintf(f, "dump-drcs = %s\n", B8(opt_dump_drcs));

    if (opt_ass_do) {

        fprintf(f, "\n[ass]\n");
        if (opt_ass_output)
            fprintf(f, "output = \"%s\"\n", TESC(PCu8(opt_ass_output)));
        if (opt_ass_font_path)
            fprintf(f, "font = \"%s\"\n", TESC(PCu8(opt_ass_font_path)));
        if (opt_ass_font_face)
            fprintf(f, "font-face = \"%s\"\n", TESC(PCu8(opt_ass_font_face)));
        fprintf(f, "force-bold = %s\n", B8(opt_ass_force_bold));
        fprintf(f, "force-border = %s\n", B8(opt_ass_force_border));
        fprintf(f, "merge-regions = %s\n", B8(opt_ass_merge_regions));
        fprintf(f, "debug-boxes = %s\n", B8(opt_ass_debug_boxes));
        fprintf(f, "center-spacing = %s\n", B8(opt_ass_center_spacing));
        fprintf(f, "constant-spacing = %d\n", opt_ass_constant_spacing);
        fprintf(f, "shift-ruby = %s\n", B8(opt_ass_shift_ruby));
        fprintf(f, "fs-adjust = %s\n", B8(opt_ass_fs_adjust));
    }

    if (opt_srt_do) {
        fprintf(f, "\n[srt]\n");
        if (opt_srt_output)
            fprintf(f, "output = \"%s\"\n", TESC(PCu8(opt_srt_output)));
        fprintf(f, "tags = %s\n", B8(opt_srt_tags));
        fprintf(f, "furi = %s\n", B8(opt_srt_furi));
    }

    fclose(f);
    return NOERR;
}

static enum error parse_toml(toml_table_t *toml)
{
    const toml_table_t *subt;
    toml_value_t val;

    val = toml_table_string(toml, "output");
    if (val.ok) {
        opt_output = u8PCmem(val.u.s);
    }

    val = toml_table_string(toml, "logging");
    if (val.ok) {
        if (strcmp(val.u.s, "verbose") == 0) {
            opt_log_level = LOG_DEBUG;
        } else if (strcmp(val.u.s, "quiet") == 0) {
            opt_log_level = LOG_ERROR;
        }
        free(val.u.s);
    }

    val = toml_table_bool(toml, "dump-drcs");
    if (val.ok) {
        opt_dump_drcs = val.u.b;
    }

    subt = toml_table_table(toml, "ass");
    if (subt) {
        opt_ass_do = true;

        val = toml_table_string(subt, "output");
        if (val.ok) {
            opt_ass_output = u8PCmem(val.u.s);
        }

        val = toml_table_string(subt, "optimize");
        if (val.ok) {
            opt_ass_optimize = val.u.b;
        }

        val = toml_table_string(subt, "font");
        if (val.ok) {
            opt_ass_font_path = u8PCmem(val.u.s);
        }

        val = toml_table_string(subt, "font-face");
        if (val.ok) {
            opt_ass_font_face = u8PCmem(val.u.s);
        }

        val = toml_table_int(subt, "force-bold");
        if (val.ok) {
            opt_ass_force_bold = val.u.b;
        }

        val = toml_table_int(subt, "force-border");
        if (val.ok) {
            opt_ass_force_border = val.u.b;
        }

        val = toml_table_int(subt, "merge-regions");
        if (val.ok) {
            opt_ass_merge_regions = val.u.b;
        }

        val = toml_table_int(subt, "debug-boxes");
        if (val.ok) {
            opt_ass_debug_boxes = val.u.b;
        }

        val = toml_table_int(subt, "center-spacing");
        if (val.ok) {
            opt_ass_center_spacing = val.u.b;
        }

        val = toml_table_int(subt, "constant-spacing");
        if (val.ok) {
            opt_ass_constant_spacing = val.u.i;
        }

        val = toml_table_int(subt, "shift-ruby");
        if (val.ok) {
            opt_ass_shift_ruby = val.u.b;
        }

        val = toml_table_int(subt, "fs-adjust");
        if (val.ok) {
            opt_ass_fs_adjust = val.u.b;
        }
    }

    subt = toml_table_table(toml, "srt");
    if (subt) {
        opt_srt_do = true;

        val = toml_table_string(subt, "output");
        if (val.ok) {
            opt_srt_output = u8PCmem(val.u.s);
        }

        val = toml_table_int(subt, "tags");
        if (val.ok) {
            opt_srt_tags = val.u.b;
        }

        val = toml_table_int(subt, "furi");
        if (val.ok) {
            opt_srt_furi = val.u.b;
        }
    }

    return NOERR;
}

static enum error parse_config_file(const pchar *path)
{
    char errorbuf[256];
    enum error err = ERR_UNDEF;
    FILE *f = pfopen(path, PSTR("rb"));

    if (!f) {
        enum error err = -errno;
        log_error("Failed to open config file '%s': %s\n", path, error_to_string(err));
        return ERR_OPT_BAD_ARG;
    }

	toml_table_t *toml = toml_parse_file(f, errorbuf, sizeof(errorbuf));
    if (toml == NULL) {
        log_error("Failed to parse config file %s\n", u8PC(errorbuf));
        err = ERR_OPT_BAD_ARG;
        goto end;
    }

    err = parse_toml(toml);

    toml_free(toml);
end:
    fclose(f);
    return err;
}

static void advance_input_files(int argc, pchar **argv)
{
    for (; optind < argc; optind++) {
        pchar *c = argv[optind];
        if (pstrcmp(c, PSTR("ass")) == 0 || pstrcmp(c, PSTR("srt")) == 0 || c[0] == PSTR('-'))
            break;
        arrput(opt_input_files, c);
    }
}

static enum error parse_config_global_opts(int argc, pchar **argv)
{
    const pchar *config_file_path = NULL;
    /* Don't print invalid option messages */
    opterr = 0;

    for (;;) {
        int c = getopt_long(argc, argv, arg_string_pre, arg_options_pre, NULL);
        if (c == -1)
            break;

        switch (c) {
        case SOPT_CONFIG_FILE:
            config_file_path = optarg;
            break;
        case SOPT_DUMP_CONFIG:
            opt_dump_config = optarg;
            break;
        case SOPT_OUTPUT:
            nnfree(opt_output);
            opt_output = pstrdup(optarg);
            break;
        case SOPT_VERBOSE:
            opt_log_level = LOG_DEBUG;
            break;
        case SOPT_QUIET:
            opt_log_level = LOG_ERROR;
            break;
        case SOPT_DUMP_DRCS:
            opt_dump_drcs = true;
            break;
        case SOPT_HELP:
            print_help();
            return ERR_OPT_SHOULD_EXIT;
        }
    }

    if (config_file_path)
        return parse_config_file(config_file_path);
    return NOERR;
}

static enum error parse_ass_opts(int argc, pchar **argv)
{
    opt_ass_do = true;
    optind++;

    int64_t n;
    for (;;) {
        int c = getopt_long(argc, argv, arg_string_ass, arg_options_ass, NULL);
        if (c == -1)
            break;

        switch (c) {
            case SOPT_HELP:
                print_help();
                return ERR_OPT_SHOULD_EXIT;
            case SOPT_OUTPUT:
                nnfree(opt_ass_output);
                opt_ass_output = pstrdup(optarg);
                break;
            case SOPT_ASS_NO_OPTIMIZE:
                opt_ass_optimize = false;
                break;
            case SOPT_ASS_FONT_PATH:
                nnfree(opt_ass_font_path);
                opt_ass_font_path = pstrdup(optarg);
                break;
            case SOPT_ASS_FONT_FACE:
                nnfree(opt_ass_font_face);
                opt_ass_font_face = pstrdup(optarg);
                break;
            case SOPT_ASS_FORCE_BORDER:
                opt_ass_force_border = true;
                break;
            case SOPT_ASS_FORCE_BOLD:
                opt_ass_force_bold = true;
                break;
            case SOPT_ASS_MERGE_REGIONS:
                opt_ass_merge_regions = true;
                break;
            case SOPT_ASS_DEBUG_BOXES:
                opt_ass_debug_boxes = true;
                break;
            case SOPT_ASS_NO_CENTER_SPACING:
                opt_ass_center_spacing = false;
                break;
            case SOPT_ASS_CONSTANT_SPACING:
                errno = 0;
                n = strtol(optarg, NULL, 10);
                if (errno == 0) {
                    opt_ass_constant_spacing = n;
                    opt_ass_center_spacing = false;
                } else {
                    return ERR_OPT_BAD_ARG;
                }
                break;
            case SOPT_ASS_SHIFT_RUBY:
                opt_ass_shift_ruby = true;
                break;
            case SOPT_ASS_FS_ADJUST:
                opt_ass_fs_adjust = true;
                break;
            default:
                return ERR_OPT_UNKNOWN_OPT;
        }
    }

    return NOERR;
}

static enum error parse_srt_opts(int argc, pchar **argv)
{
    optind++;

    opt_srt_do = true;
    for (;;) {
        int c = getopt_long(argc, argv, arg_string_srt, arg_options_srt, NULL);
        if (c == -1)
            break;

        switch (c) {
            case SOPT_HELP:
                print_help();
                return ERR_OPT_SHOULD_EXIT;
            case SOPT_OUTPUT:
                nnfree(opt_srt_output);
                opt_srt_output = pstrdup(optarg);
                break;
            case SOPT_SRT_TAGS:
                opt_srt_tags = true;
                break;
            case SOPT_SRT_FURI:
                opt_srt_furi = true;
                break;
            default:
                return ERR_OPT_UNKNOWN_OPT;
        }
    }

    return NOERR;
}

enum error opt_check_valid()
{
    if ((opt_ass_do == false && opt_srt_do == false) && (opt_dump_drcs == false)) {
        log_error("At least one output format needs to be specified\n");
        return ERR_OPT_BAD_ARG;
    }
    if (opt_output == NULL) {
        opt_output = get_current_dir_name();
        assert(opt_output);
    }
    if (opt_ass_do && opt_ass_output == NULL) {
        opt_ass_output = pstrdup(opt_output);
    }
    if (opt_srt_do && opt_srt_output == NULL) {
        opt_srt_output = pstrdup(opt_output);
    }

    if (opt_ass_do && opt_ass_font_path == NULL) {
        log_error("Must give a font for .ass output\n");
        return ERR_OPT_BAD_ARG;
    }

    struct pstat s;
    int n;
    if (opt_ass_do) {
        n = pstatfn(opt_ass_output, &s);
        if (n != 0) {
            if (errno == ENOENT) {
                opt_ass_output_dir = arrlen(opt_input_files) > 1;
            } else {
                pperror(PSTR("Failed to check output path"));
                assert(false);
            }
        } else if (!S_ISDIR(s.st_mode) && arrlen(opt_input_files) > 1) {
            log_error("Output for .ass multi-file is not a directory\n");
            return ERR_OPT_BAD_ARG;
        } else if (S_ISDIR(s.st_mode)) {
            opt_ass_output_dir = true;
        }
    }
    if (opt_srt_do) {

        n = pstatfn(opt_srt_output, &s);
        if (n != 0) {
            if (errno == ENOENT) {
                opt_srt_output_dir = arrlen(opt_input_files) > 1;
            } else {
                pperror(PSTR("Failed to check output path"));
                assert(false);
            }
        } else if (!S_ISDIR(s.st_mode) && arrlen(opt_input_files) > 1) {
            log_error("Output for .srt multi-file is not a directory\n");
            return ERR_OPT_BAD_ARG;
        } else if (S_ISDIR(s.st_mode)) {
            opt_srt_output_dir = true;
        }
    }

    if (opt_ass_do) {
        if (opt_ass_center_spacing && opt_ass_constant_spacing != -1) {
            log_error("Center spacing and constant spacing cannot both be set\n");
            return ERR_OPT_BAD_ARG;
        }
        if (opt_ass_shift_ruby && opt_ass_constant_spacing == -1) {
            log_error("Ruby shift does not makes sense when using calculated spacing\n");
            return ERR_OPT_BAD_ARG;
        }
    }

    return NOERR;
}

/*
 * Parse command line arguments
 */
enum error opts_parse_cmdline(int argc, pchar **argv)
{
    enum error err = ERR_UNDEF;

    err = parse_config_global_opts(argc, argv);
    if (err != NOERR)
        goto fail;

    /* Print invalid opton messages */
    opterr = 1;

    advance_input_files(argc, argv);
    while (optind < argc) {
        if (pstrcmp(argv[optind], PSTR("ass")) == 0) {
            err = parse_ass_opts(argc, argv);
        } else if (pstrcmp(argv[optind], PSTR("srt")) == 0) {
            err = parse_srt_opts(argc, argv);
        } else {
            err = parse_config_global_opts(argc, argv);
        }

        if (err != NOERR)
            goto fail;

        advance_input_files(argc, argv);
    }

    err = opt_check_valid();
    if (err != NOERR)
        goto fail;

    if (opt_dump_config) {
        err = dump_config(opt_dump_config);
        if (err == NOERR)
            return ERR_OPT_SHOULD_EXIT;
        return err;
    }

    return NOERR;
fail:
    opts_free();
    return err;
}

/* Free any resources allocated by config */
void opts_free()
{
    if (opt_ass_font_path)
        free((void*)opt_ass_font_path);
    if (opt_ass_font_face)
        free((void*)opt_ass_font_face);
    arrfree(opt_input_files);
    if (opt_output)
        free(opt_output);
    if (opt_ass_output)
        free(opt_ass_output);
    if (opt_srt_output)
        free(opt_srt_output);
}
