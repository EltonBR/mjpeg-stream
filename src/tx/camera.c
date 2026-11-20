#include "camera.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int xioctl(int fd, unsigned long request, void *arg)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r < 0 && errno == EINTR);
    return r;
}

int camera_uses_mjpeg(const struct camera *camera)
{
    return camera->pixfmt == V4L2_PIX_FMT_MJPEG;
}

int camera_wait_fd(const struct camera *camera)
{
    return camera->fd;
}

int camera_open(struct config *cfg, struct camera *camera)
{
    int fd;
    struct v4l2_format fmt;
    struct v4l2_streamparm parm;
    struct v4l2_requestbuffers req;
    unsigned i;
    enum v4l2_buf_type type;

    memset(camera, 0, sizeof(*camera));
    camera->fd = -1;

    fd = open(cfg->device, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror(cfg->device);
        return -1;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = cfg->width;
    fmt.fmt.pix.height = cfg->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0 ||
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
        if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0 ||
            fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) {
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        }
        if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV &&
            xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
            perror("VIDIOC_S_FMT");
            close(fd);
            return -1;
        }
    }

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG &&
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV &&
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) {
        fprintf(stderr, "camera retornou formato nao suportado: %.4s\n",
                (char *)&fmt.fmt.pix.pixelformat);
        close(fd);
        return -1;
    }

    cfg->width = fmt.fmt.pix.width;
    cfg->height = fmt.fmt.pix.height;
    if (cfg->width == 0 || cfg->height == 0 ||
        (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV &&
         (cfg->width & 1u) != 0)) {
        fprintf(stderr, "camera retornou dimensoes invalidas\n");
        close(fd);
        return -1;
    }
    if (cfg->width > 65500u || cfg->height > 65500u ||
        (size_t)cfg->width > SIZE_MAX / 3u / (size_t)cfg->height) {
        fprintf(stderr, "camera retornou dimensoes grandes demais\n");
        close(fd);
        return -1;
    }

    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = cfg->fps;
    (void)xioctl(fd, VIDIOC_S_PARM, &parm);

    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        close(fd);
        return -1;
    }

    camera->buffers = calloc(req.count, sizeof(*camera->buffers));
    if (!camera->buffers) {
        close(fd);
        return -1;
    }

    for (i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF");
            goto fail_buffers;
        }
        camera->buffers[i].length = buf.length;
        camera->buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, fd, buf.m.offset);
        if (camera->buffers[i].start == MAP_FAILED) {
            perror("mmap");
            goto fail_buffers;
        }
        camera->buffer_count++;
        if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            goto fail_buffers;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        goto fail_buffers;
    }

    fprintf(stderr, "camera: %ux%u formato %.4s\n",
            fmt.fmt.pix.width, fmt.fmt.pix.height,
            (char *)&fmt.fmt.pix.pixelformat);

    camera->fd = fd;
    camera->buffer_count = req.count;
    camera->pixfmt = fmt.fmt.pix.pixelformat;
    camera->bytesperline = fmt.fmt.pix.bytesperline;
    return 0;

fail_buffers:
    while (camera->buffer_count > 0) {
        unsigned index = --camera->buffer_count;
        munmap(camera->buffers[index].start, camera->buffers[index].length);
    }
    free(camera->buffers);
    camera->buffers = NULL;
    close(fd);
    return -1;
}

int camera_next_frame(struct camera *camera, struct camera_frame *frame)
{
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (xioctl(camera->fd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) {
            return 0;
        }
        perror("VIDIOC_DQBUF");
        return -1;
    }

    if (buf.index >= camera->buffer_count ||
        buf.bytesused > camera->buffers[buf.index].length) {
        fprintf(stderr, "camera: buffer V4L2 invalido\n");
        return -1;
    }

    frame->index = buf.index;
    frame->data = camera->buffers[buf.index].start;
    frame->bytesused = buf.bytesused;
    return 1;
}

int camera_requeue_frame(struct camera *camera, const struct camera_frame *frame)
{
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = frame->index;
    if (xioctl(camera->fd, VIDIOC_QBUF, &buf) < 0) {
        perror("VIDIOC_QBUF");
        return -1;
    }
    return 0;
}

void camera_close(struct camera *camera)
{
    if (camera->fd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        (void)xioctl(camera->fd, VIDIOC_STREAMOFF, &type);
    }

    if (camera->buffers) {
        unsigned i;
        for (i = 0; i < camera->buffer_count; i++) {
            if (camera->buffers[i].start &&
                camera->buffers[i].start != MAP_FAILED) {
                munmap(camera->buffers[i].start, camera->buffers[i].length);
            }
        }
    }

    free(camera->buffers);
    camera->buffers = NULL;
    camera->buffer_count = 0;
    if (camera->fd >= 0) {
        close(camera->fd);
        camera->fd = -1;
    }
}
