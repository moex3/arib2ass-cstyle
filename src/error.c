#include "error.h"
#include <string.h>

const pchar *error_strings[] = {
#define X(erre) \
    [erre] = PSTR(#erre),

    ERR_ENUMS(X)
#undef X
};

const pchar *error_to_string(enum error err)
{
    if ((signed)err >= 0)
        return error_strings[err];
    return pstrerror(-err);
}
