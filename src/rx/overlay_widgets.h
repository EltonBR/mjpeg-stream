#ifndef MJPEG_RX_OVERLAY_WIDGETS_H
#define MJPEG_RX_OVERLAY_WIDGETS_H

#include "widgets/common/widget_common.h"

GPtrArray *overlay_widgets_new(void);
void overlay_widgets_load(GPtrArray *widgets, struct json_object *array);
void overlay_widgets_draw(GPtrArray *widgets, cairo_t *cr,
                          const struct overlay_render_context *ctx);

#endif
