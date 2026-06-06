#ifndef MJPEG_TX_CAMERA_H
#define MJPEG_TX_CAMERA_H

#include "config.h"

#include <stddef.h>
#include <stdint.h>

struct tx_buffer {
    void *start;
    size_t length;
};

struct camera {
    int fd;
    struct tx_buffer *buffers;
    unsigned buffer_count;
    uint32_t pixfmt;
};

struct camera_frame {
    unsigned index;
    void *data;
    size_t bytesused;
};

int camera_open(struct config *cfg, struct camera *camera);
void camera_close(struct camera *camera);
int camera_next_frame(struct camera *camera, struct camera_frame *frame);
int camera_requeue_frame(struct camera *camera, const struct camera_frame *frame);
int camera_wait_fd(const struct camera *camera);
int camera_uses_mjpeg(const struct camera *camera);

#endif
