#ifndef MJPEG_RX_OVERLAY_INTERNAL_H
#define MJPEG_RX_OVERLAY_INTERNAL_H

#include "overlay.h"

void overlay_element_free(gpointer data);
int overlay_parse_color(const char *text, double *r, double *g, double *b);

#endif
