#ifndef ARIB2ASS_TSDECODE_H
#define ARIB2ASS_TSDECODE_H

#include <sys/types.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "error.h"

struct tsdecode {
    /* The opened file by avformat */
    AVFormatContext * avformat_context;
    /* The index of the ARIB caption stream in the file */
    int               caption_stream_idx, video_stream_idx;
    int64_t           file_size;

#ifdef _WIN32
    AVIOContext *ioc;
    FILE *fh;
#endif
};

/*
 * Opens a .ts file for decoding.
 * Returns a tsdecode struct if successfull
 * You need to call tsdecode_free()
 */
enum error tsdecode_open_file(const pchar *fpath, struct tsdecode *out);
void tsdecode_free(struct tsdecode *tsd);

/*
 * Start to decode the arib stream, and call the callback 'cb' for every packet decoded
 * If 'cb' returns anything other than NOERR, the loop stops, and that value is returned
 */
typedef enum error (*tsdecode_decode_packets_cb)(AVPacket *packet, void *arg);
enum error tsdecode_decode_packets(struct tsdecode *tsd, tsdecode_decode_packets_cb cb, void *arg);
time_t     tsdecode_get_video_length(const struct tsdecode *tsd);

#endif /* ARIB2ASS_TSDECODE_H */
