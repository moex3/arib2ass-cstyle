#include "png.h"
#include <assert.h>
#include "log.h"

#include <libavcodec/avcodec.h>

/* Encode using libavcodec as we already link with it
 * (We could also use stb_image_write.h)
 */

enum error png_encode(int w, int h, int depth, uint8_t *px, size_t pxsize,
        uint8_t **out_buf, size_t *out_buf_size)
{
    enum error err = ERR_LIBAV;
    int ret;
    AVFrame *frame = NULL;
    AVCodecContext *ctx = NULL;
    AVPacket *pkt = NULL;
    const AVCodec *codec;
    uint8_t *buf = NULL;
    size_t buf_size = 0;

    assert(depth == 2);
    assert(pxsize == ((w * h) / 4));

    codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (codec == NULL)
        goto end;

    ctx = avcodec_alloc_context3(codec);
    if (ctx == NULL)
        goto end;

    ctx->time_base = (AVRational){1, 1};
    ctx->width = w;
    ctx->height = h;
    //ctx->pix_fmt = AV_PIX_FMT_MONOBLACK;
    ctx->pix_fmt = AV_PIX_FMT_GRAY8;

    ret = avcodec_open2(ctx, codec, NULL);
    if (ret < 0)
        goto end;

    frame = av_frame_alloc();
    assert(frame);
    frame->format = ctx->pix_fmt;
    frame->width  = ctx->width;
    frame->height = ctx->height;

    pkt = av_packet_alloc();
    if (pkt == NULL)
        goto end;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0)
        goto end;

    for (int y = 0, di = 0; y < h; y++) {
        for (int x = 0; x < w; x++, di++) {
            /* 2 bits per pixel in the drcs pixel data, convert that to 8 bits per pixel */
            int byte = di / 4;
            int bit = (di % 4) * 2;
            /* 6 - because we need to switch the bit order around,
             * and we take 2 bits each loop */
            uint8_t c = px[byte] >> (6 - bit);
            /* Bits are still reversed, that's why this is in a weird order */
            static const uint8_t lum[] = { 0, 0xAA, 0x55, 0xFF };
            frame->data[0][y * frame->linesize[0] + x] = lum[c & 0x3];
        }
    }

    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0)
        goto end;

    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        assert(ret >= 0);

        size_t nsize = buf_size + pkt->size;
        uint8_t *nbuf = reallocarray(buf, nsize, 1);
        assert(nbuf);

        memcpy(&nbuf[buf_size], pkt->data, pkt->size);
        buf = nbuf;
        buf_size = nsize;

        av_packet_unref(pkt);
    }

    *out_buf = buf;
    *out_buf_size = buf_size;

    err = NOERR;
end:
    if (ctx)
        avcodec_free_context(&ctx);
    if (frame)
        av_frame_free(&frame);
    if (pkt)
        av_packet_free(&pkt);
    if (err != NOERR)
        free(buf);

    return err;
}
