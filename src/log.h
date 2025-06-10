#ifndef ARIB2ASS_LOG_H
#define ARIB2ASS_LOG_H
#include <stdarg.h>
#include <stdint.h>
#include "platform.h"

enum log_level {
    LOG_DEBUG,
    LOG_INFO,
    LOG_MSG,
    LOG_WARNING,
    LOG_ERROR,
    LOG_NONE,
};

#define STRINGIFY(STRING) #STRING
#define XSTRINGIFY(x) STRINGIFY(x)
#if 0
#define log_debug(fmt, ...) log_disp(LOG_DEBUG,     PSTR2(__FILE__) PSTR(":") PSTR2(XSTRINGIFY( __LINE__ )) PSTR(": ") PSTR(fmt), ## __VA_ARGS__)
#define log_info(fmt, ...) log_disp(LOG_INFO,       PSTR2(__FILE__) PSTR(":") PSTR2(XSTRINGIFY( __LINE__ )) PSTR(": ") PSTR(fmt), ## __VA_ARGS__)
#define log_warning(fmt, ...) log_disp(LOG_WARNING, PSTR2(__FILE__) PSTR(":") PSTR2(XSTRINGIFY( __LINE__ )) PSTR(": ") PSTR(fmt), ## __VA_ARGS__)
#else
#define log_debug(fmt, ...) log_disp(LOG_DEBUG,     PSTR(fmt), ## __VA_ARGS__)
#define log_info(fmt, ...) log_disp(LOG_INFO,       PSTR(fmt), ## __VA_ARGS__)
#define log_warning(fmt, ...) log_disp(LOG_WARNING, PSTR(fmt), ## __VA_ARGS__)
#endif
#define log_user(fmt, ...) log_disp(LOG_MSG,        PSTR(fmt), ## __VA_ARGS__)
#define log_error(fmt, ...) log_disp(LOG_ERROR,     PSTR(fmt), ## __VA_ARGS__)

int log_disp(enum log_level level, const pchar* format, ...)
#ifndef _WIN32
	__attribute__((format(printf, 2, 3)))
#endif
;

enum log_progress_state {
    LPS_BEGIN,
    LPS_UPDATE,
    LPS_END,
};

/* The value of val depends on state */
int log_progress(enum log_progress_state state, void *val);

#endif /* ARIB2ASS_LOG_H */
