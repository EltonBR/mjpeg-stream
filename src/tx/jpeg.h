#ifndef MJPEG_TX_JPEG_H
#define MJPEG_TX_JPEG_H

void yuyv_to_rgb(const unsigned char *src, unsigned char *dst,
                 unsigned width, unsigned height);
int encode_jpeg(const unsigned char *rgb, unsigned width, unsigned height,
                int quality, unsigned char **out, unsigned long *out_len);

#endif
