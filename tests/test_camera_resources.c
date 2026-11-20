#include "camera.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define FAKE_FD 73
#define BUFFER_LENGTH 4096U
#define MAX_MAPS 4U

enum failure_point {
    FAIL_NONE,
    FAIL_QUERYBUF,
    FAIL_MMAP,
    FAIL_QBUF,
    FAIL_STREAMON
};

static enum failure_point failure;
static unsigned failure_index;
static unsigned mmap_count;
static unsigned munmap_count;
static unsigned close_count;
static unsigned streamoff_count;
static void *mapped[MAX_MAPS];
static size_t unmapped_lengths[MAX_MAPS];
static unsigned char fake_storage[MAX_MAPS][BUFFER_LENGTH];

int __wrap_open(const char *path, int flags, ...)
{
    assert(strcmp(path, "/dev/fake-camera") == 0);
    assert((flags & O_RDWR) != 0);
    assert((flags & O_NONBLOCK) != 0);
    return FAKE_FD;
}

int __wrap_close(int fd)
{
    assert(fd == FAKE_FD);
    close_count++;
    return 0;
}

int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list args;
    void *arg;

    assert(fd == FAKE_FD);
    va_start(args, request);
    arg = va_arg(args, void *);
    va_end(args);

    if (request == VIDIOC_S_FMT) {
        struct v4l2_format *fmt = arg;
        fmt->fmt.pix.width = 640;
        fmt->fmt.pix.height = 480;
        fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        return 0;
    }
    if (request == VIDIOC_S_PARM) {
        return 0;
    }
    if (request == VIDIOC_REQBUFS) {
        struct v4l2_requestbuffers *req = arg;
        req->count = MAX_MAPS;
        return 0;
    }
    if (request == VIDIOC_QUERYBUF) {
        struct v4l2_buffer *buf = arg;
        if (failure == FAIL_QUERYBUF && buf->index == failure_index) {
            errno = EIO;
            return -1;
        }
        buf->length = BUFFER_LENGTH;
        buf->m.offset = buf->index * BUFFER_LENGTH;
        return 0;
    }
    if (request == VIDIOC_QBUF) {
        struct v4l2_buffer *buf = arg;
        if (failure == FAIL_QBUF && buf->index == failure_index) {
            errno = EIO;
            return -1;
        }
        return 0;
    }
    if (request == VIDIOC_STREAMON) {
        if (failure == FAIL_STREAMON) {
            errno = EIO;
            return -1;
        }
        return 0;
    }
    if (request == VIDIOC_STREAMOFF) {
        streamoff_count++;
        return 0;
    }

    assert(!"unexpected ioctl request");
    return -1;
}

void *__wrap_mmap(void *addr, size_t length, int prot, int flags, int fd,
                  off_t offset)
{
    unsigned index = mmap_count;

    (void)addr;
    assert(fd == FAKE_FD);
    assert(length == BUFFER_LENGTH);
    assert(prot == (PROT_READ | PROT_WRITE));
    assert(flags == MAP_SHARED);
    assert(offset == (off_t)(index * BUFFER_LENGTH));
    if (failure == FAIL_MMAP && index == failure_index) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    assert(index < MAX_MAPS);
    mapped[index] = fake_storage[index];
    mmap_count++;
    return mapped[index];
}

int __wrap_munmap(void *addr, size_t length)
{
    unsigned i;

    for (i = 0; i < mmap_count; i++) {
        if (mapped[i] == addr) {
            assert(unmapped_lengths[i] == 0);
            assert(length == BUFFER_LENGTH);
            unmapped_lengths[i] = length;
            munmap_count++;
            return 0;
        }
    }
    assert(!"munmap called for an unknown mapping");
    return -1;
}

static void reset_mocks(enum failure_point fail_at, unsigned index)
{
    failure = fail_at;
    failure_index = index;
    mmap_count = 0;
    munmap_count = 0;
    close_count = 0;
    streamoff_count = 0;
    memset(mapped, 0, sizeof(mapped));
    memset(unmapped_lengths, 0, sizeof(unmapped_lengths));
}

static void run_open_failure(enum failure_point fail_at, unsigned index,
                             unsigned expected_maps)
{
    struct config cfg = {0};
    struct camera camera;

    reset_mocks(fail_at, index);
    cfg.device = "/dev/fake-camera";
    cfg.width = 640;
    cfg.height = 480;
    cfg.fps = 30;

    assert(camera_open(&cfg, &camera) == -1);
    assert(mmap_count == expected_maps);
    assert(munmap_count == expected_maps);
    assert(close_count == 1);
    assert(streamoff_count == 0);
    assert(camera.fd == -1);
    assert(camera.buffers == NULL);
    assert(camera.buffer_count == 0);
}

static void test_open_failure_rollback(void)
{
    run_open_failure(FAIL_QUERYBUF, 2, 2);
    run_open_failure(FAIL_MMAP, 2, 2);
    run_open_failure(FAIL_QBUF, 2, 3);
    run_open_failure(FAIL_STREAMON, 0, MAX_MAPS);
}

static void test_camera_close_releases_resources(void)
{
    struct config cfg = {0};
    struct camera camera;
    unsigned i;

    reset_mocks(FAIL_NONE, 0);
    cfg.device = "/dev/fake-camera";
    cfg.width = 640;
    cfg.height = 480;
    cfg.fps = 30;

    assert(camera_open(&cfg, &camera) == 0);
    assert(camera.fd == FAKE_FD);
    assert(camera.buffer_count == MAX_MAPS);
    assert(camera.buffers != NULL);
    camera_close(&camera);

    assert(streamoff_count == 1);
    assert(munmap_count == MAX_MAPS);
    assert(close_count == 1);
    assert(camera.fd == -1);
    assert(camera.buffers == NULL);
    assert(camera.buffer_count == 0);
    for (i = 0; i < MAX_MAPS; i++) {
        assert(unmapped_lengths[i] == BUFFER_LENGTH);
    }
}

int main(void)
{
    test_open_failure_rollback();
    test_camera_close_releases_resources();
    puts("camera mmap rollback and close resource tests passed");
    return 0;
}
