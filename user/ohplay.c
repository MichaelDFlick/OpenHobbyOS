#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"

#include "syscall.h"
#include "xnx.h"

#define PLAYER_QUEUE_SIZE 8

struct player_frame {
    AVFrame *frame;
    int64_t pts_us;
};

struct frame_queue {
    struct player_frame items[PLAYER_QUEUE_SIZE];
    volatile unsigned int head;
    volatile unsigned int tail;
    volatile int abort_requested;
    volatile int finished;
};

struct player_state {
    const char *path;
    xnx_conn_t *conn;
    uint32_t surface_id;
    uint32_t display_w;
    uint32_t display_h;
    int video_stream_index;
    AVFormatContext *format;
    AVCodecContext *codec;
    AVRational time_base;
    int64_t fallback_step_us;
    struct frame_queue queue;
    int64_t playback_start_us;
    int64_t first_pts_us;
    uint32_t *screen;
    uint32_t *scaled;
    size_t screen_pixels;
    size_t scaled_pixels;
    int target_w;
    int target_h;
    int target_x;
    int target_y;
    struct SwsContext *sws;
};

static void sleep_us(int64_t delay_us) {
    struct linux_timespec req;

    if (delay_us <= 0) {
        return;
    }

    req.tv_sec = (i32)(delay_us / 1000000LL);
    req.tv_nsec = (i32)((delay_us % 1000000LL) * 1000LL);
    sys_nanosleep(&req, NULL);
}

static int64_t monotonic_us(void) {
    struct linux_timespec ts;

    if (sys_clock_gettime(LINUX_CLOCK_MONOTONIC, &ts) < 0) {
        return 0;
    }

    return (int64_t)ts.tv_sec * 1000000LL + (int64_t)ts.tv_nsec / 1000LL;
}

static void queue_reset(struct frame_queue *q) {
    memset(q, 0, sizeof(*q));
}

static void queue_abort(struct frame_queue *q) {
    q->abort_requested = 1;
}

static int queue_push(struct frame_queue *q, AVFrame *frame, int64_t pts_us) {
    unsigned int next;
    struct player_frame item;

    item.frame = frame;
    item.pts_us = pts_us;

    while (!q->abort_requested) {
        next = (q->head + 1u) % PLAYER_QUEUE_SIZE;
        if (next != q->tail) {
            q->items[q->head] = item;
            __sync_synchronize();
            q->head = next;
            return 0;
        }
        sys_sched_yield();
    }

    av_frame_free(&frame);
    return -1;
}

static int queue_pop(struct frame_queue *q, struct player_frame *out) {
    if (q->tail == q->head) {
        return 0;
    }

    __sync_synchronize();
    *out = q->items[q->tail];
    q->tail = (q->tail + 1u) % PLAYER_QUEUE_SIZE;
    return 1;
}

static void free_player_frame(struct player_frame *frame) {
    if (!frame || !frame->frame) {
        return;
    }
    av_frame_free(&frame->frame);
    frame->frame = NULL;
}

static void update_letterbox(struct player_state *state, int src_w, int src_h) {
    int target_w;
    int target_h;

    if (src_w <= 0 || src_h <= 0 || state->display_w == 0 || state->display_h == 0) {
        state->target_w = (int)state->display_w;
        state->target_h = (int)state->display_h;
        state->target_x = 0;
        state->target_y = 0;
        return;
    }

    if ((int64_t)src_w * (int64_t)state->display_h > (int64_t)state->display_w * (int64_t)src_h) {
        target_w = (int)state->display_w;
        target_h = (int)((int64_t)state->display_w * src_h / src_w);
    } else {
        target_h = (int)state->display_h;
        target_w = (int)((int64_t)state->display_h * src_w / src_h);
    }

    if (target_w < 1) target_w = 1;
    if (target_h < 1) target_h = 1;

    state->target_w = target_w;
    state->target_h = target_h;
    state->target_x = ((int)state->display_w - target_w) / 2;
    state->target_y = ((int)state->display_h - target_h) / 2;
}

static int ensure_scaler(struct player_state *state, int src_w, int src_h, enum AVPixelFormat src_fmt) {
    state->sws = sws_getCachedContext(state->sws,
                                       src_w, src_h, src_fmt,
                                       state->target_w, state->target_h,
                                       AV_PIX_FMT_BGRA,
                                       SWS_BILINEAR,
                                       NULL, NULL, NULL);
    return state->sws ? 0 : -1;
}

static void fill_black(struct player_state *state) {
    size_t total = (size_t)state->display_w * (size_t)state->display_h;

    for (size_t i = 0; i < total; ++i) {
        state->screen[i] = 0xFF000000u;
    }
}

static int render_frame(struct player_state *state, const AVFrame *frame) {
    uint8_t *dst_slices[4];
    int dst_stride[4];
    int src_w = frame->width;
    int src_h = frame->height;

    if (src_w <= 0 || src_h <= 0) {
        return -1;
    }

    update_letterbox(state, src_w, src_h);
    if (ensure_scaler(state, src_w, src_h, (enum AVPixelFormat)frame->format) < 0) {
        return -1;
    }

    dst_slices[0] = (uint8_t *)state->scaled;
    dst_stride[0] = state->target_w * 4;
    dst_slices[1] = dst_slices[2] = dst_slices[3] = NULL;
    dst_stride[1] = dst_stride[2] = dst_stride[3] = 0;

    if (sws_scale(state->sws,
                  (const uint8_t * const *)frame->data,
                  frame->linesize,
                  0,
                  src_h,
                  dst_slices,
                  dst_stride) < 0) {
        return -1;
    }

    fill_black(state);
    for (int y = 0; y < state->target_h; ++y) {
        uint32_t *dst_row = state->screen + ((size_t)(state->target_y + y) * state->display_w) + (size_t)state->target_x;
        const uint32_t *src_row = state->scaled + (size_t)y * (size_t)state->target_w;
        memcpy(dst_row, src_row, (size_t)state->target_w * sizeof(uint32_t));
    }

    if (xnx_write_buffer(state->conn, state->surface_id, 0, 0, state->display_w, state->display_h, state->screen) < 0) {
        return -1;
    }
    if (xnx_commit(state->conn, state->surface_id) < 0) {
        return -1;
    }

    return 0;
}

static int64_t frame_pts_us(struct player_state *state, const AVFrame *frame, int64_t fallback_index) {
    int64_t pts;

    pts = frame->best_effort_timestamp;
    if (pts == AV_NOPTS_VALUE) {
        pts = frame->pts;
    }
    if (pts == AV_NOPTS_VALUE) {
        return fallback_index * state->fallback_step_us;
    }

    return av_rescale_q(pts, state->time_base, AV_TIME_BASE_Q);
}

static unsigned int decode_thread_main(void *arg) {
    struct player_state *state = (struct player_state *)arg;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int64_t fallback_index = 0;
    int result;

    if (!packet || !frame) {
        goto out;
    }

    while (!state->queue.abort_requested) {
        result = av_read_frame(state->format, packet);
        if (result < 0) {
            break;
        }

        if (packet->stream_index == state->video_stream_index) {
            if (avcodec_send_packet(state->codec, packet) == 0) {
                for (;;) {
                    result = avcodec_receive_frame(state->codec, frame);
                    if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
                        break;
                    }
                    if (result < 0) {
                        state->queue.abort_requested = 1;
                        goto out;
                    }

                    AVFrame *copy = av_frame_clone(frame);
                    if (!copy) {
                        state->queue.abort_requested = 1;
                        goto out;
                    }

                    if (queue_push(&state->queue, copy, frame_pts_us(state, frame, fallback_index)) < 0) {
                        goto out;
                    }
                    fallback_index++;
                    av_frame_unref(frame);
                }
            }
        }

        av_packet_unref(packet);
    }

    avcodec_send_packet(state->codec, NULL);
    for (;;) {
        result = avcodec_receive_frame(state->codec, frame);
        if (result == AVERROR_EOF || result == AVERROR(EAGAIN)) {
            break;
        }
        if (result < 0) {
            state->queue.abort_requested = 1;
            goto out;
        }

        AVFrame *copy = av_frame_clone(frame);
        if (!copy) {
            state->queue.abort_requested = 1;
            goto out;
        }
        if (queue_push(&state->queue, copy, frame_pts_us(state, frame, fallback_index)) < 0) {
            goto out;
        }
        fallback_index++;
        av_frame_unref(frame);
    }

out:
    state->queue.finished = 1;
    if (packet) {
        av_packet_free(&packet);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    return 0;
}

static int handle_events(xnx_conn_t *conn) {
    struct xnx_event event;

    while (xnx_poll_event(conn, &event) > 0) {
        if (event.type == XNX_EVENT_KEY && event.data.key.pressed) {
            switch ((int)event.data.key.keycode) {
                case 3:
                case 'q':
                case 'Q':
                case 27:
                    return 1;
                default:
                    break;
            }
        }
    }

    return 0;
}

static const char *basename_ptr(const char *path) {
    const char *name = path;

    if (!path) {
        return "";
    }

    for (const char *p = path; *p; ++p) {
        if (*p == '/') {
            name = p + 1;
        }
    }

    return *name ? name : path;
}

static void cleanup_player(struct player_state *state) {
    struct player_frame frame;

    while (queue_pop(&state->queue, &frame)) {
        free_player_frame(&frame);
    }

    if (state->sws) {
        sws_freeContext(state->sws);
        state->sws = NULL;
    }
    if (state->scaled) {
        free(state->scaled);
        state->scaled = NULL;
    }
    if (state->screen) {
        free(state->screen);
        state->screen = NULL;
    }
    if (state->codec) {
        avcodec_free_context(&state->codec);
    }
    if (state->format) {
        avformat_close_input(&state->format);
    }
    if (state->conn) {
        if (state->surface_id != 0) {
            xnx_destroy_surface(state->conn, state->surface_id);
        }
        xnx_disconnect(state->conn);
        state->conn = NULL;
    }
}

int main(int argc, char **argv) {
    struct player_state state;
    unsigned int decode_tid = 0;
    int exit_code = 1;
    int status = 0;
    AVStream *video_stream = NULL;
    const AVCodec *codec = NULL;
    int stream_index;
    int source_w = 0;
    int source_h = 0;

    memset(&state, 0, sizeof(state));
    queue_reset(&state.queue);

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s <video-file>\n", basename_ptr(argv[0]));
        return 1;
    }

    state.path = argv[1];
    av_log_set_level(AV_LOG_ERROR);

    state.conn = xnx_connect(NULL);
    if (!state.conn) {
        fprintf(stderr, "ohplay: failed to connect to XNX\n");
        goto out;
    }

    if (xnx_get_display_info(state.conn, &state.display_w, &state.display_h) < 0 ||
        state.display_w == 0 || state.display_h == 0) {
        state.display_w = 1024;
        state.display_h = 768;
    }

    if (xnx_create_surface(state.conn, state.display_w, state.display_h, &state.surface_id) < 0) {
        fprintf(stderr, "ohplay: failed to create XNX surface\n");
        goto out;
    }
    xnx_set_title(state.conn, state.surface_id, basename_ptr(state.path));
    xnx_set_geometry(state.conn, state.surface_id, 0, 0, state.display_w, state.display_h);

    if (avformat_open_input(&state.format, state.path, NULL, NULL) < 0) {
        fprintf(stderr, "ohplay: failed to open input file\n");
        goto out;
    }
    if (avformat_find_stream_info(state.format, NULL) < 0) {
        fprintf(stderr, "ohplay: failed to read stream info\n");
        goto out;
    }

    stream_index = av_find_best_stream(state.format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (stream_index < 0) {
        fprintf(stderr, "ohplay: no video stream found\n");
        goto out;
    }
    state.video_stream_index = stream_index;
    video_stream = state.format->streams[stream_index];
    state.time_base = video_stream->time_base;
    {
        AVRational rate = av_guess_frame_rate(state.format, video_stream, NULL);
        if (rate.num <= 0 || rate.den <= 0) {
            rate = video_stream->avg_frame_rate;
        }
        if (rate.num <= 0 || rate.den <= 0) {
            rate = video_stream->r_frame_rate;
        }
        if (rate.num > 0 && rate.den > 0) {
            state.fallback_step_us = (int64_t)rate.den * 1000000LL / (int64_t)rate.num;
        } else {
            state.fallback_step_us = 40000LL;
        }
    }

    codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "ohplay: unsupported codec\n");
        goto out;
    }
    state.codec = avcodec_alloc_context3(codec);
    if (!state.codec) {
        fprintf(stderr, "ohplay: out of memory\n");
        goto out;
    }
    if (avcodec_parameters_to_context(state.codec, video_stream->codecpar) < 0) {
        fprintf(stderr, "ohplay: failed to configure decoder\n");
        goto out;
    }
    if (avcodec_open2(state.codec, codec, NULL) < 0) {
        fprintf(stderr, "ohplay: failed to open decoder\n");
        goto out;
    }

    source_w = state.codec->width;
    source_h = state.codec->height;
    if (source_w <= 0 || source_h <= 0) {
        fprintf(stderr, "ohplay: invalid video dimensions\n");
        goto out;
    }

    state.screen_pixels = (size_t)state.display_w * (size_t)state.display_h;
    state.scaled_pixels = (size_t)state.display_w * (size_t)state.display_h;
    state.screen = calloc(state.screen_pixels, sizeof(uint32_t));
    state.scaled = calloc(state.scaled_pixels, sizeof(uint32_t));
    if (!state.screen || !state.scaled) {
        fprintf(stderr, "ohplay: out of memory\n");
        goto out;
    }

    if (sys_thread_create(&decode_tid, NULL, decode_thread_main, &state) < 0) {
        fprintf(stderr, "ohplay: failed to start decode thread\n");
        goto out;
    }

    state.playback_start_us = monotonic_us();
    state.first_pts_us = AV_NOPTS_VALUE;

    for (;;) {
        struct player_frame frame;

        if (handle_events(state.conn)) {
            state.queue.abort_requested = 1;
            break;
        }

        if (!queue_pop(&state.queue, &frame)) {
            if (state.queue.finished) {
                break;
            }
            sys_sched_yield();
            continue;
        }

        if (state.first_pts_us == AV_NOPTS_VALUE) {
            state.first_pts_us = frame.pts_us;
            state.playback_start_us = monotonic_us();
        }

        if (frame.pts_us != AV_NOPTS_VALUE && state.first_pts_us != AV_NOPTS_VALUE) {
            int64_t target_us = state.playback_start_us + (frame.pts_us - state.first_pts_us);
            int64_t now_us = monotonic_us();
            if (target_us > now_us) {
                sleep_us(target_us - now_us);
            }
        }

        if (render_frame(&state, frame.frame) < 0) {
            free_player_frame(&frame);
            state.queue.abort_requested = 1;
            break;
        }

        free_player_frame(&frame);
    }

    exit_code = 0;

out:
    queue_abort(&state.queue);
    if (decode_tid != 0) {
        sys_thread_join(decode_tid, &status);
    }
    cleanup_player(&state);
    return exit_code;
}
