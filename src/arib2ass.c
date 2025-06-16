#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "srt.h"
#include "font.h"
#include "ass.h"
#include "error.h"
#include "tsdecode.h"
#include "subobj.h"
#include "log.h"
#include "util.h"
#include "platform.h"
#include "opts.h"

struct decode_ctx {
    struct subobj_ctx *sctx;
    float fsize;
};

enum output_type {
    ASS,
    SRT
};

static enum error decode(AVPacket *packet, void *arg)
{
    struct decode_ctx *c = arg;
    float r = ((float)packet->pos) / c->fsize;
    log_progress(LPS_UPDATE, &r);
    return subobj_parse_from_packet(c->sctx, packet);
}

static void setup_output_path(pchar path[256])
{
    struct pstat s;
    int n;
    pchar *sep;

    n = pstatfn(path, &s);
    if (n == 0)
        return;
    if (errno != ENOENT) {
        pperror(PSTR("Cannot setup output path"));
        assert(false);
        return;
    }

    sep = pstrrchr(path, PATHSPECC);
    if (sep == NULL)
        return;
    *sep = PSTR('\0');

    n = mkdir_p(path);
    if (n != 0) {
        pperror(PSTR("Cannot mkdir"));
        assert(false);
    }
    *sep = PATHSPECC;
}

static void create_output_path(enum output_type ot, const pchar *input, pchar out[256])
{
    pchar buf[256];
    const pchar *ext[] = {
        [ASS] = PSTR(".ass"),
        [SRT] = PSTR(".srt"),
    };

    bool isdir;
    const pchar *outpath;
    if (ot == ASS) {
        isdir = opt_ass_output_dir;
        outpath = opt_ass_output;
    } else if (ot == SRT) {
        isdir = opt_srt_output_dir;
        outpath = opt_srt_output;
    } else {
        assert(false);
        return;
    }

    if (isdir == false) {
        psnprintf(out, 256, PSTR("%s"), outpath);
        setup_output_path(out);
        goto end;
    }

    size_t ilen = pstrlen(input);
    assert(ilen < 255);
    assert(ilen > 4);
    memcpy(buf, input, ilen * sizeof(*input));
    buf[ilen] = '\0';

    pchar *bn = basename(buf);
    size_t blen = pstrlen(bn);

    const pchar *tsext = PSTR(".ts");
    const size_t tsextclen = pstrlen(tsext);
    if (memcmp(&bn[blen - tsextclen], tsext, tsextclen * sizeof(*tsext)) == 0) {
        blen -= tsextclen;
    }

    mkdir_p(outpath);
    psnprintf(out, 256, PSTR("%s%c%.*s%s"), outpath, PATHSPECC, (int)blen, bn, ext[ot]);
end:
    return;
}

int fnmain(int argc, pchar **argv)
{
    enum error err;
    struct tsdecode tsd = {0};
    struct subobj_ctx sctx = {0};
    struct decode_ctx dctx;
    pchar outpath[256], mbuf[512], measure_str[32];
    time_t measure_ms;
    bool had_error = false;

#ifdef __linux__
    /* For wcrtomb */
    char *lset = setlocale(LC_CTYPE, "C.utf8");
    assert(lset && "Can't set UTF-8 locale");
#elif defined(_WIN32)
    (void)_setmode(_fileno(stdout), _O_U16TEXT);
    (void)_setmode(_fileno(stderr), _O_U16TEXT);
#endif

    err = opts_parse_cmdline(argc, argv);
    if (err != NOERR) {
        return 1;
    }

    if (arrlen(opt_input_files) <= 0) {
        opts_free();
        return 1;
    }

    font_init();

    for (intptr_t i_i = 0; i_i < arrlen(opt_input_files); i_i++) {
        const pchar *input = opt_input_files[i_i];
        log_user("Processing input file: %s\n", input);

        err = tsdecode_open_file(input, &tsd);
        if (err != NOERR) {
            had_error = true;
            goto end_file;
        }

        dctx = (struct decode_ctx){
            .sctx = &sctx,
            .fsize = (float)tsd.file_size,
        };

        err = subobj_create(&sctx, &tsd);
        if (err != NOERR) {
            had_error = true;
            goto end;
        }

        static const pchar took_ms_fmt[] = PSTR("took %") PSTR2(PRIi64) PSTR(" ms");
        log_progress(LPS_BEGIN, PSTR("Reading .ts file"));
        MEASURE_START(tsdec);

        err = tsdecode_decode_packets(&tsd, decode, &dctx);

        MEASURE_END(tsdec, measure_ms);
        psnprintf(measure_str, sizeof(measure_str), took_ms_fmt, measure_ms);
        log_progress(LPS_END, measure_str);

        if (err != NOERR) {
            had_error = true;
            goto end;
        }

        if (opt_srt_do) {
            create_output_path(SRT, input, outpath);
            psnprintf(mbuf, sizeof(mbuf), PSTR("Writing .srt file to %s"), outpath);

            log_progress(LPS_BEGIN, mbuf);
            MEASURE_START(srtw);

            err = srt_write(&sctx, outpath);

            MEASURE_END(srtw, measure_ms);
            psnprintf(measure_str, sizeof(measure_str), took_ms_fmt, measure_ms);
            log_progress(LPS_END, measure_str);

            if (err != NOERR) {
                log_error("Failed to generate srt file: %s\n", error_to_string(err));
            }
        }

        if (opt_srt_do && opt_ass_do) {
            log_progress(LPS_BEGIN, PSTR("Resetting subobj"));
            MEASURE_START(srtw);

            subobj_reset_mod(&sctx);

            MEASURE_END(srtw, measure_ms);
            psnprintf(measure_str, sizeof(measure_str), took_ms_fmt, measure_ms);
            log_progress(LPS_END, measure_str);
        }

        if (opt_ass_do) {
            create_output_path(ASS, input, outpath);
            psnprintf(mbuf, sizeof(mbuf), PSTR("Writing .ass file to %s"), outpath);

            log_progress(LPS_BEGIN, mbuf);
            MEASURE_START(assw);

            err = ass_write(&sctx, outpath);

            MEASURE_END(assw, measure_ms);
            psnprintf(measure_str, sizeof(measure_str), took_ms_fmt, measure_ms);
            log_progress(LPS_END, measure_str);

            if (err != NOERR) {
                log_error("Failed to generate ass file: %s\n", error_to_string(err));
            }
        }

end:
        subobj_destroy(&sctx);
end_file:
        tsdecode_free(&tsd);
    }

    font_dinit();
    opts_free();
    if (had_error && err == NOERR) {
        err = ERR_UNDEF;
    }

    return err == NOERR ? 0 : 1;
}
