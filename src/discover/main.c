#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int xioctl(int fd, unsigned long request, void *arg)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r < 0 && errno == EINTR);
    return r;
}

static void fourcc_to_str(uint32_t fourcc, char out[5])
{
    out[0] = (char)(fourcc & 0xffu);
    out[1] = (char)((fourcc >> 8) & 0xffu);
    out[2] = (char)((fourcc >> 16) & 0xffu);
    out[3] = (char)((fourcc >> 24) & 0xffu);
    out[4] = '\0';
}

static double fps_from_interval(const struct v4l2_fract *f)
{
    if (f->numerator == 0) {
        return 0.0;
    }
    return (double)f->denominator / (double)f->numerator;
}

static void print_stepwise_intervals(const struct v4l2_frmivalenum *ival)
{
    double min_fps = fps_from_interval(&ival->stepwise.max);
    double max_fps = fps_from_interval(&ival->stepwise.min);
    printf("        fps: %.2f..%.2f step %u/%u s\n",
           min_fps, max_fps,
           ival->stepwise.step.numerator,
           ival->stepwise.step.denominator);
}

static void print_intervals(int fd, uint32_t pixelformat,
                            uint32_t width, uint32_t height)
{
    struct v4l2_frmivalenum ival;

    memset(&ival, 0, sizeof(ival));
    ival.pixel_format = pixelformat;
    ival.width = width;
    ival.height = height;

    for (ival.index = 0; ; ival.index++) {
        if (xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival) < 0) {
            if (errno == EINVAL && ival.index == 0) {
                printf("        fps: nao informado pelo driver\n");
            }
            return;
        }

        if (ival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            printf("        fps: %.2f (%u/%u s)\n",
                   fps_from_interval(&ival.discrete),
                   ival.discrete.numerator,
                   ival.discrete.denominator);
        } else if (ival.type == V4L2_FRMIVAL_TYPE_STEPWISE ||
                   ival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
            print_stepwise_intervals(&ival);
            return;
        }
    }
}

static void print_stepwise_sizes(const struct v4l2_frmsizeenum *size)
{
    printf("    resolucao: %ux%u..%ux%u step %ux%u\n",
           size->stepwise.min_width,
           size->stepwise.min_height,
           size->stepwise.max_width,
           size->stepwise.max_height,
           size->stepwise.step_width,
           size->stepwise.step_height);
}

static void print_sizes(int fd, uint32_t pixelformat)
{
    struct v4l2_frmsizeenum size;

    memset(&size, 0, sizeof(size));
    size.pixel_format = pixelformat;

    for (size.index = 0; ; size.index++) {
        if (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &size) < 0) {
            if (errno == EINVAL && size.index == 0) {
                printf("    resolucoes: nao informado pelo driver\n");
            }
            return;
        }

        if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            printf("    resolucao: %ux%u\n",
                   size.discrete.width, size.discrete.height);
            print_intervals(fd, pixelformat,
                            size.discrete.width, size.discrete.height);
        } else if (size.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
                   size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
            print_stepwise_sizes(&size);
            return;
        }
    }
}

static void print_caps(int fd)
{
    struct v4l2_capability cap;
    unsigned int caps;

    memset(&cap, 0, sizeof(cap));
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("VIDIOC_QUERYCAP");
        return;
    }

    caps = cap.capabilities;
    if (cap.capabilities & V4L2_CAP_DEVICE_CAPS) {
        caps = cap.device_caps;
    }

    printf("driver: %s\n", cap.driver);
    printf("card: %s\n", cap.card);
    printf("bus: %s\n", cap.bus_info);
    printf("capture: %s\n", (caps & V4L2_CAP_VIDEO_CAPTURE) ? "sim" : "nao");
    printf("streaming: %s\n", (caps & V4L2_CAP_STREAMING) ? "sim" : "nao");
}

static void print_formats(int fd)
{
    struct v4l2_fmtdesc fmt;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (fmt.index = 0; ; fmt.index++) {
        char fourcc[5];
        if (xioctl(fd, VIDIOC_ENUM_FMT, &fmt) < 0) {
            if (errno == EINVAL && fmt.index == 0) {
                printf("nenhum formato retornado pelo driver\n");
            }
            return;
        }

        fourcc_to_str(fmt.pixelformat, fourcc);
        printf("\nformato: %s", fourcc);
        if (fmt.flags & V4L2_FMT_FLAG_COMPRESSED) {
            printf(" compressed");
        }
        if (fmt.flags & V4L2_FMT_FLAG_EMULATED) {
            printf(" emulated");
        }
        printf("\n");
        printf("  descricao: %s\n", fmt.description);
        print_sizes(fd, fmt.pixelformat);
    }
}

int main(int argc, char **argv)
{
    const char *device = "/dev/video0";
    int fd;

    if (argc > 2) {
        fprintf(stderr, "Uso: %s [/dev/videoX]\n", argv[0]);
        return 2;
    }
    if (argc == 2) {
        device = argv[1];
    }

    fd = open(device, O_RDWR);
    if (fd < 0) {
        perror(device);
        return 1;
    }

    printf("device: %s\n", device);
    print_caps(fd);
    print_formats(fd);

    close(fd);
    return 0;
}
