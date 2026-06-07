#ifndef MJPEG_RX_IMAGE_WIDGET_H
#define MJPEG_RX_IMAGE_WIDGET_H

#include "../common/widget_common.h"

struct overlay_widget *image_widget_parse(struct json_object *obj);
void image_widget_draw(struct overlay_widget *widget, cairo_t *cr,
                       const struct overlay_render_context *ctx);

#endif
