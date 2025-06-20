#include "tsdecode.h"

#include <assert.h>
#include <sys/stat.h>
#include <libavutil/opt.h>

#include "log.h"
#include "opts.h"
#include "util.h"

/* Can be increased if the subtitle stream is not found */
#define PROBESIZE ( 64*1024*1024 )

static void find_stream_infos(AVFormatContext *avformat_context, int *out_sub_idx, int *out_video_idx)
{
    int  ret;

    av_opt_set_int(avformat_context, "probesize", PROBESIZE, 0);

    ret = avformat_find_stream_info(avformat_context, NULL);
    if (ret < 0) {
        log_debug("Failed to find stream info: %s\n", u8PC(av_err2str(ret)));
        return;
    }

    for (size_t i = 0; i < avformat_context->nb_streams; i++) {
        AVStream* stream = avformat_context->streams[i];
        AVCodecParameters* codec_params = stream->codecpar;

        if (codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            *out_video_idx = stream->index;
        }

        if (codec_params->codec_type == AVMEDIA_TYPE_SUBTITLE && codec_params->codec_id == AV_CODEC_ID_ARIB_CAPTION) {
            *out_sub_idx = stream->index;
        }
    }
}

#ifdef _WIN32

static int read_ts_file_packet(void *opaque, uint8_t *buf, int buf_size)
{
    struct tsdecode *tsd = opaque;

    //log_debug("In read ts packet\n");
    size_t r = fread(buf, 1, buf_size, tsd->fh);
    if (r == 0) {
        if (feof(tsd->fh)) {
            return AVERROR_EOF;
        }
        return AVERROR_EXTERNAL;
    }

    return r;
}

static enum error open_ts_file(const pchar *fpath, struct tsdecode *tsd)
{
    FILE *f = pfopen(fpath, PSTR("rb"));
    if (f == NULL)
        return -errno;
    tsd->fh = f;
    return NOERR;
}

#endif

static int open_av_file(const pchar *fpath, AVFormatContext **out_avc, struct tsdecode *tsd)
{
#ifdef _WIN32
    AVFormatContext *avc = NULL;
    AVIOContext *ioc = NULL;
    const size_t buffer_size = 4096;
    uint8_t *avio_buffer = NULL;
    enum error err;

    avc = avformat_alloc_context();
    assert(avc);

    avio_buffer = av_malloc(buffer_size);
    assert(avio_buffer);

    ioc = avio_alloc_context(avio_buffer, buffer_size, 0, tsd, &read_ts_file_packet, NULL, NULL);
    assert(ioc);

    err = open_ts_file(fpath, tsd);
    if (err != NOERR) {
        avformat_free_context(avc);
        av_free(avio_buffer);
        avio_context_free(&ioc);
        //log_error("Failed to open input file: %s\n", error_to_string(err));
        return AVERROR(ENOENT);
    }

    tsd->ioc = ioc;
    avc->pb = ioc;
    *out_avc = avc;
    return avformat_open_input(out_avc, NULL, NULL, NULL);
#else
    return avformat_open_input(out_avc, fpath, NULL, NULL);
#endif
}

enum error tsdecode_open_file(const pchar *fpath, struct tsdecode *out)
{
    AVFormatContext * avformat_context = NULL;
    struct pstat st;
    int               ret, caption_stream_idx = -1, video_stream_idx = -1;

    if (opt_log_level > LOG_DEBUG)
        av_log_set_level(AV_LOG_QUIET);

    ret = open_av_file(fpath, &avformat_context, out);
    if (ret < 0) {
        log_error("Failed to open file %s: %s\n", fpath, u8PC(av_err2str(ret)));
        return ERR_LIBAV;
    }
    ret = pstatfn(fpath, &st);
    assert(ret == 0);

    find_stream_infos(avformat_context, &caption_stream_idx, &video_stream_idx);
    if (caption_stream_idx == -1 || video_stream_idx == -1) {
        log_error("caption stream not found in file\n");
        tsdecode_free(out);
        return ERR_NO_CAPTION_STREAM;
    }
    log_info("ARIB Caption stream was found at index: %d\n", caption_stream_idx);

    *out = (struct tsdecode){
        .avformat_context   = avformat_context,
        .caption_stream_idx = caption_stream_idx,
        .video_stream_idx   = video_stream_idx,
        .file_size          = st.st_size,
#ifdef _WIN32
        .ioc                = out->ioc,
        .fh                 = out->fh,
#endif
    };
    return NOERR;
}

void tsdecode_free(struct tsdecode *tsd)
{
    if (tsd->avformat_context)
        avformat_close_input(&tsd->avformat_context);

#ifdef _WIN32
    if (tsd->ioc) {
        av_freep(&tsd->ioc->buffer);
	}
	avio_context_free(&tsd->ioc);
    if (tsd->fh)
		fclose(tsd->fh);
#endif

    memset(tsd, 0, sizeof(*tsd));
}

enum error tsdecode_decode_packets(struct tsdecode *tsd, tsdecode_decode_packets_cb cb, void *arg)
{
    AVPacket   packet = {0};
    int        ret = 0;
    enum error err = NOERR;

    while ((ret = av_read_frame(tsd->avformat_context, &packet)) == 0) {
        if (packet.stream_index == tsd->caption_stream_idx) {
            AVStream* cap_stream = tsd->avformat_context->streams[tsd->caption_stream_idx];
            AVStream* vid_stream = tsd->avformat_context->streams[tsd->video_stream_idx];

            packet.pts = MAX(packet.pts - vid_stream->start_time, 0);
            packet.dts = MAX(packet.dts - vid_stream->start_time, 0);
            av_packet_rescale_ts(&packet, cap_stream->time_base, (AVRational){1, 1000});

            err = cb(&packet, arg);
            av_packet_unref(&packet);

            if (err != NOERR) {
                break;
            }
        } else {
            av_packet_unref(&packet);
        }
    }

    if (err != NOERR)
        return err;
    if (err == NOERR && ret == AVERROR_EOF)
        return NOERR;
    log_error("Failed to reach EOF while reading packets: %d - %d %s\n", err, ret, u8PC(av_err2str(ret)));
    return ERR_LIBAV;
}

time_t tsdecode_get_video_length(const struct tsdecode *tsd)
{
    const AVStream *stream = tsd->avformat_context->streams[tsd->video_stream_idx];
    time_t d = stream->duration;
    return av_rescale_q(d, stream->time_base, (AVRational){1, 1000});
}
