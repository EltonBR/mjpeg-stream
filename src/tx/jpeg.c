#include "jpeg.h"

#include <stddef.h>
#include <stdio.h>
#include <jpeglib.h>

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
        row_pointer[0] = (JSAMPROW)&rgb[cinfo.next_scanline * width * 3u];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return 0;
}
