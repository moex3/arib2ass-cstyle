#ifndef ARIB2ASS_ERROR_H
#define ARIB2ASS_ERROR_H
#include "platform.h"

/*
 * And error is either a value defined here if positive,
 * or an errno value if negative.
 * 0 for no error
 */
#define ERR_ENUMS(X) \
    X(NOERR) \
    X(ERR_LIBAV) \
    X(ERR_NO_CAPTION_STREAM) \
    X(ERR_ARIBCC) \
    X(ERR_OPT_BAD_ARG) \
    X(ERR_OPT_SHOULD_EXIT) \
    X(ERR_OPT_UNKNOWN_OPT) \
    X(ERR_FREETYPE) \
    X(ERR_FONT_FACE_NOT_FOUND) \
    X(ERR_INVALID_DRCS_REPLACEMENT) \
    X(ERR_NO_DRCS_REPLACEMENT_FOUNT) \
\
    X(ERR_UNDEF) \



enum error {
#define X(e) e,
    ERR_ENUMS(X)
#undef X
};

const pchar *error_to_string(enum error err);

#endif /* ARIB2ASS_ERROR_H */
