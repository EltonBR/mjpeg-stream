#include "jpeg.h"

#include <assert.h>
#include <limits.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_rgb_stride_and_minimum_bytesused(void)
{
    const unsigned char input[14] = {
        1, 2, 3, 4, 5, 6, 90, 91,
        7, 8, 9, 10, 11, 12
    };
    unsigned char output[12] = {0};

    assert(frame_to_rgb(V4L2_PIX_FMT_RGB24, input, sizeof(input), 8,
                        2, 2, output) == 0);
    assert(memcmp(output, "\1\2\3\4\5\6\7\10\11\12\13\14", 12) == 0);
    assert(frame_to_rgb(V4L2_PIX_FMT_RGB24, input, 13, 8,
                        2, 2, output) < 0);
    assert(frame_to_rgb(V4L2_PIX_FMT_RGB24, input, sizeof(input), 5,
                        2, 2, output) < 0);
}

static void test_yuyv_stride_and_dimensions(void)
{
    const unsigned char input[10] = {
        100, 128, 100, 128, 77, 78,
        200, 128, 200, 128
    };
    unsigned char output[12] = {0};

    assert(frame_to_rgb(V4L2_PIX_FMT_YUYV, input, sizeof(input), 6,
                        2, 2, output) == 0);
    assert(output[0] == 100 && output[1] == 100 && output[2] == 100);
    assert(output[6] == 200 && output[7] == 200 && output[8] == 200);
    assert(frame_to_rgb(V4L2_PIX_FMT_YUYV, input, 9, 6,
                        2, 2, output) < 0);
    assert(frame_to_rgb(V4L2_PIX_FMT_YUYV, input, sizeof(input), 6,
                        3, 2, output) < 0);
}

static void test_size_overflow_and_empty_mjpeg(void)
{
    size_t size = 0;
    const unsigned char byte = 0xff;

    assert(rgb_frame_size(2, 3, &size) == 0 && size == 18);
    assert(rgb_frame_size(0, 3, &size) < 0);
    assert(rgb_frame_size(UINT_MAX, UINT_MAX, &size) < 0);
    assert(!mjpeg_frame_valid(&byte, 0));
    assert(mjpeg_frame_valid(&byte, 1));
    assert(!mjpeg_frame_valid(NULL, 1));
    if (SIZE_MAX > ULONG_MAX) {
        assert(!mjpeg_frame_valid(&byte, (size_t)ULONG_MAX + 1u));
    }
}

int main(void)
{
    test_rgb_stride_and_minimum_bytesused();
    test_yuyv_stride_and_dimensions();
    test_size_overflow_and_empty_mjpeg();
    puts("camera RGB/YUYV frame validation tests passed");
    return 0;
}
