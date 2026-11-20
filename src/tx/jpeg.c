#include "jpeg.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <linux/videodev2.h>
#include <jpeglib.h>

static int checked_mul_size(size_t a, size_t b, size_t *result)
{
    if (a != 0 && b > SIZE_MAX / a) {
        return -1;
    }
    *result = a * b;
    return 0;
}

int rgb_frame_size(unsigned width, unsigned height, size_t *size)
{
    size_t pixels;

    if (!size || width == 0 || height == 0 ||
        width > JPEG_MAX_DIMENSION || height > JPEG_MAX_DIMENSION ||
        checked_mul_size((size_t)width, (size_t)height, &pixels) < 0 ||
        checked_mul_size(pixels, 3u, size) < 0) {
        return -1;
    }
    return 0;
}

int mjpeg_frame_valid(const unsigned char *data, size_t bytesused)
{
    return data != NULL && bytesused > 0 && bytesused <= ULONG_MAX;
}

int frame_to_rgb(uint32_t pixfmt, const unsigned char *src, size_t bytesused,
                 size_t bytesperline, unsigned width, unsigned height,
                 unsigned char *dst)
{
    size_t min_row, stride, required, preceding_rows, rgb_size;
    unsigned y;

    if (!src || !dst || width == 0 || height == 0 ||
        width > JPEG_MAX_DIMENSION || height > JPEG_MAX_DIMENSION) {
        return -1;
    }
    if (pixfmt == V4L2_PIX_FMT_RGB24) {
        if (checked_mul_size(width, 3u, &min_row) < 0) {
            return -1;
        }
    } else if (pixfmt == V4L2_PIX_FMT_YUYV) {
        if ((width & 1u) != 0 || checked_mul_size(width, 2u, &min_row) < 0) {
            return -1;
        }
    } else {
        return -1;
    }

    stride = bytesperline ? bytesperline : min_row;
    if (stride < min_row ||
        checked_mul_size((size_t)height - 1u, stride, &preceding_rows) < 0 ||
        preceding_rows > SIZE_MAX - min_row) {
        return -1;
    }
    required = preceding_rows + min_row;
    if (bytesused < required ||
        rgb_frame_size(width, height, &rgb_size) < 0) {
        return -1;
    }

    if (pixfmt == V4L2_PIX_FMT_RGB24) {
        size_t dst_stride = (size_t)width * 3u;
        for (y = 0; y < height; y++) {
            size_t src_offset = (size_t)y * stride;
            memcpy(dst + (size_t)y * dst_stride, src + src_offset, dst_stride);
        }
    } else {
        for (y = 0; y < height; y++) {
            yuyv_to_rgb(src + (size_t)y * stride,
                        dst + (size_t)y * (size_t)width * 3u, width, 1);
        }
    }
    return 0;
}

void yuyv_to_rgb(const unsigned char *src, unsigned char *dst,
                 unsigned width, unsigned height)
{
    size_t pixels = (size_t)width * height;
    size_t i;
    for (i = 0; i < pixels; i += 2) {
        int y0 = src[0];
        int u = src[1] - 128;
        int y1 = src[2];
        int v = src[3] - 128;
        int r, g, b;

#define CLAMP(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))
        r = y0 + ((359 * v) >> 8);
        g = y0 - ((88 * u + 183 * v) >> 8);
        b = y0 + ((454 * u) >> 8);
        dst[0] = (unsigned char)CLAMP(r);
        dst[1] = (unsigned char)CLAMP(g);
        dst[2] = (unsigned char)CLAMP(b);

        r = y1 + ((359 * v) >> 8);
        g = y1 - ((88 * u + 183 * v) >> 8);
        b = y1 + ((454 * u) >> 8);
        dst[3] = (unsigned char)CLAMP(r);
        dst[4] = (unsigned char)CLAMP(g);
        dst[5] = (unsigned char)CLAMP(b);
#undef CLAMP

        src += 4;
        dst += 6;
    }
}

int encode_jpeg(const unsigned char *rgb, unsigned width, unsigned height,
                int quality, unsigned char **out, unsigned long *out_len)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    size_t rgb_size;

    if (!rgb || !out || !out_len || quality < 0 || quality > 100 ||
        rgb_frame_size(width, height, &rgb_size) < 0) {
        return -1;
    }
    (void)rgb_size;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, out, out_len);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = (JSAMPROW)&rgb[(size_t)cinfo.next_scanline * width * 3u];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return 0;
}
