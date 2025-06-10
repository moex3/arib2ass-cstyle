#ifndef A2AC_SRT_H
#define A2AC_SRT_H
#include <stdint.h>
#include "error.h"
#include "subobj.h"
#include "platform.h"

enum error srt_write(const struct subobj_ctx *sctx, const pchar *filepath);

#endif /* A2AC_SRT_H */
