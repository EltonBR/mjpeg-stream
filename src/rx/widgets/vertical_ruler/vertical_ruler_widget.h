#ifndef MJPEG_RX_VERTICAL_RULER_WIDGET_H
#define MJPEG_RX_VERTICAL_RULER_WIDGET_H

#include "../common/widget_common.h"

struct overlay_widget *vertical_ruler_widget_parse(struct json_object *obj);
struct overlay_widget *horizontal_ruler_widget_parse(struct json_object *obj);
void vertical_ruler_widget_draw(struct overlay_widget *widget, cairo_t *cr,
                                const struct overlay_render_context *ctx);
void horizontal_ruler_widget_draw(struct overlay_widget *widget, cairo_t *cr,
                                  const struct overlay_render_context *ctx);

#endif
