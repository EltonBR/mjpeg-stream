#include "image_widget.h"

#include <math.h>
#include <stdio.h>

struct overlay_widget *image_widget_parse(struct json_object *obj)
{
    struct overlay_widget *widget = widget_base(obj, WIDGET_IMAGE);
    if (!widget) {
        return NULL;
    }
    widget->file = widget_dup_json_string(obj, "file", NULL);
    widget->rotation_expr = widget_dup_json_string(obj, "rotation", NULL);
    if (!widget_valid_image_path(widget->file)) {
        fprintf(stderr, "overlay: widget image com file invalido ignorado\n");
        overlay_widget_free(widget);
        return NULL;
    }
    return widget;
}

void image_widget_draw(struct overlay_widget *widget, cairo_t *cr,
                       const struct overlay_render_context *ctx)
{
    double x;
    double y;
    double w;
    double h;
    double left;
    double top;
    double angle;

    load_widget_pixbuf(ctx, widget);
    if (!widget->pixbuf) {
        return;
    }

    x = ctx->frame_x + widget_coord_value(widget->x, ctx->frame_w);
    y = ctx->frame_y + widget_coord_value(widget->y, ctx->frame_h);
    w = widget_coord_value(widget->w, ctx->frame_w);
    h = widget_coord_value(widget->h, ctx->frame_h);
    if (w <= 0.0) {
        w = gdk_pixbuf_get_width(widget->pixbuf);
    }
    if (h <= 0.0) {
        h = gdk_pixbuf_get_height(widget->pixbuf);
    }
    angle = widget_template_number(ctx, widget->rotation_expr, 0.0) * G_PI / 180.0;
    widget_anchor_origin(widget->anchor, x, y, w, h, &left, &top);

    cairo_save(cr);
    cairo_translate(cr, left + w / 2.0, top + h / 2.0);
    cairo_rotate(cr, angle);
    cairo_scale(cr, w / gdk_pixbuf_get_width(widget->pixbuf),
                h / gdk_pixbuf_get_height(widget->pixbuf));
    gdk_cairo_set_source_pixbuf(cr, widget->pixbuf,
                                -gdk_pixbuf_get_width(widget->pixbuf) / 2.0,
                                -gdk_pixbuf_get_height(widget->pixbuf) / 2.0);
    cairo_paint_with_alpha(cr, widget->alpha);
    cairo_restore(cr);
}
