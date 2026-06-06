#include "camera.h"
#include "config.h"
#include "jpeg.h"
#include "stream.h"

#include <errno.h>
#include <linux/videodev2.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void on_signal(int signo)
{
    (void)signo;
    running = 0;
}

static int wait_for_events(const struct camera *camera,
                           const struct stream_server *stream,
                           fd_set *fds)
{
    int camera_fd = camera_wait_fd(camera);
    int stream_fd = stream_wait_fd(stream);
    int max_fd = camera_fd > stream_fd ? camera_fd : stream_fd;
    struct timeval tv;

    FD_ZERO(fds);
    FD_SET(camera_fd, fds);
    if (stream->protocol != PROTO_UDP) {
        FD_SET(stream_fd, fds);
    }

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    return select(max_fd + 1, fds, NULL, NULL, &tv);
}

static int frame_to_jpeg(const struct config *cfg, const struct camera *camera,
                         const struct camera_frame *frame, unsigned char *rgb,
                         unsigned char **jpeg, unsigned long *jpeg_len)
{
    if (camera_uses_mjpeg(camera)) {
        *jpeg = frame->data;
        *jpeg_len = (unsigned long)frame->bytesused;
        return 1;
    }

    if (camera->pixfmt == V4L2_PIX_FMT_YUYV) {
        yuyv_to_rgb(frame->data, rgb, cfg->width, cfg->height);
    } else if (camera->pixfmt == V4L2_PIX_FMT_RGB24) {
        memcpy(rgb, frame->data, (size_t)cfg->width * cfg->height * 3u);
    } else {
        fprintf(stderr, "formato V4L2 nao suportado\n");
        return -1;
    }

    return encode_jpeg(rgb, cfg->width, cfg->height, cfg->quality,
                       jpeg, jpeg_len) == 0 ? 0 : -1;
}

int main(int argc, char **argv)
{
    struct config cfg;
    struct camera camera;
    struct stream_server stream;
    unsigned char *rgb = NULL;
    unsigned int frame_id = 1;
    int exit_code = 0;

    if (tx_parse_args(argc, argv, &cfg) < 0) {
        tx_usage(argv[0]);
        return 2;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    if (camera_open(&cfg, &camera) < 0) {
        return 1;
    }

    if (!camera_uses_mjpeg(&camera)) {
        rgb = malloc((size_t)cfg.width * cfg.height * 3u);
        if (!rgb) {
            perror("malloc rgb");
            camera_close(&camera);
            return 1;
        }
    }

    if (stream_open(&stream, &cfg) < 0) {
        free(rgb);
        camera_close(&camera);
        return 1;
    }

    while (running) {
        fd_set fds;
        struct camera_frame frame;
        unsigned char *jpeg = NULL;
        unsigned long jpeg_len = 0;
        int jpeg_owned;
        int rc = wait_for_events(&camera, &stream, &fds);

        if (rc <= 0) {
            continue;
        }

        if (stream.protocol != PROTO_UDP &&
            FD_ISSET(stream_wait_fd(&stream), &fds)) {
            stream_accept_pending(&stream);
        }

        if (!FD_ISSET(camera_wait_fd(&camera), &fds)) {
            continue;
        }

        rc = camera_next_frame(&camera, &frame);
        if (rc < 0) {
            exit_code = 1;
            break;
        }
        if (rc == 0) {
            continue;
        }

        jpeg_owned = frame_to_jpeg(&cfg, &camera, &frame, rgb,
                                   &jpeg, &jpeg_len);
        if (jpeg_owned >= 0 &&
            stream_send_frame(&stream, jpeg, jpeg_len, &frame_id) < 0) {
            exit_code = 1;
        }
        if (jpeg_owned == 0) {
            free(jpeg);
        }

        if (camera_requeue_frame(&camera, &frame) < 0) {
            exit_code = 1;
            break;
        }
        if (exit_code != 0) {
            break;
        }
    }

    stream_close(&stream);
    camera_close(&camera);
    free(rgb);
    return exit_code;
}
