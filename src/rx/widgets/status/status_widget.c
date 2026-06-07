#include "status_widget.h"

struct overlay_widget *status_widget_parse(struct json_object *obj)
{
    struct overlay_widget *widget = widget_base(obj, WIDGET_STATUS);
    if (widget) {
        widget->text = widget_dup_json_string(obj, "text", "");
    }
    return widget;
}

void status_widget_draw(struct overlay_widget *widget, cairo_t *cr,
                        const struct overlay_render_context *ctx)
{
    char *text = widget_expand_template(ctx, widget->text);
    double x = ctx->frame_x + widget_coord_value(widget->x, ctx->frame_w);
    double y = ctx->frame_y + widget_coord_value(widget->y, ctx->frame_h);
    widget_draw_text(cr, ctx, text, x, y, widget->size, widget->alpha, widget->anchor);
    g_free(text);
}
