#ifndef MJPEG_TX_JPEG_H
#define MJPEG_TX_JPEG_H

#include <stddef.h>
#include <stdint.h>

int rgb_frame_size(unsigned width, unsigned height, size_t *size);
int mjpeg_frame_valid(const unsigned char *data, size_t bytesused);
int frame_to_rgb(uint32_t pixfmt, const unsigned char *src, size_t bytesused,
                 size_t bytesperline, unsigned width, unsigned height,
                 unsigned char *dst);
void yuyv_to_rgb(const unsigned char *src, unsigned char *dst,
                 unsigned width, unsigned height);
int encode_jpeg(const unsigned char *rgb, unsigned width, unsigned height,
                int quality, unsigned char **out, unsigned long *out_len);

#endif
