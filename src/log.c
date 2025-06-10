#include "log.h"
#include "opts.h"
#include "platform.h"

#include <stdio.h>
#include <assert.h>
#include <time.h>

#define PROG_UPDATE_MS 100
static const pchar prog_loop[] = PSTR("\\|/-");
static struct prog_state {
    bool started;
    float progress;
    const pchar *str;
    int str_len, loop_i;
    struct ptimespec last;
} prog_state = {
    .started = false,
    .progress = 0,
    .str = NULL,
    .str_len = 0,
    .loop_i = 0,
    .last = { 0 },
};


static void log_progress_newline();

int log_disp(enum log_level level, const pchar *fmt, ...)
{
    if (level < opt_log_level)
        return 0;

    if (prog_state.started) {
        /* move cursor to the beginning and erase the line */
        pfprintf(stderr, PSTR("\x1b[1G\x1b[2K"));
    }

    int n;
    va_list ap;
    va_start(ap, fmt);
    n = pvfprintf(stderr, fmt, ap);
    va_end(ap);

    if (prog_state.started) {
        log_progress_newline();
    }
    return n;
}

int log_progress(enum log_progress_state state, void *val)
{
    if (opt_log_level > LOG_MSG)
        return 0;

    struct ptimespec now;
    if (state == LPS_BEGIN) {
        prog_state = (struct prog_state){
            .started = true,
            .str = val,
            .str_len = pfprintf(stderr, PSTR("%s "), (pchar*)val),
        };
        PLATFORM_CURRENT_TIMESPEC(&prog_state.last);
        return prog_state.str_len;
    } else if (state == LPS_END) {
        prog_state.started = false;
        return pfprintf(stderr, PSTR("\x1b[%dG%s %s\n"), 1, prog_state.str, (pchar*)val);
    }
    assert(state == LPS_UPDATE);

	PLATFORM_CURRENT_TIMESPEC(&now);
    int diff_ms = (now.tv_sec - prog_state.last.tv_sec) * 1000 + ((now.tv_nsec - prog_state.last.tv_nsec) / 1000000);
    if (diff_ms < PROG_UPDATE_MS)
        return 0;
    prog_state.last = now;

    prog_state.progress = *(float*)val;
    int n = pfprintf(stderr, PSTR("\x1b[%dG%05.2f%% [%c]  "), prog_state.str_len + 1, prog_state.progress * 100.0f, prog_loop[prog_state.loop_i++ % 4]);
    fflush(stderr);
    return n;
}

static void log_progress_newline()
{
    pfprintf(stderr, PSTR("%s "), prog_state.str);
    log_progress(LPS_UPDATE, &prog_state.progress);
}
