#include "vertical_ruler_widget.h"

#include <math.h>
#include <stdio.h>

struct overlay_widget *vertical_ruler_widget_parse(struct json_object *obj)
{
    struct overlay_widget *widget = widget_base(obj, WIDGET_VERTICAL_RULER);
    if (!widget) {
        return NULL;
    }

    widget->value_expr = widget_dup_json_string(obj, "value", "{angle}");
    widget->label = widget_dup_json_string(obj, "label", "");
    widget->suffix = widget_dup_json_string(obj, "suffix", "");
    widget->min_value = widget_json_get_double_default(obj, "min", -90.0);
    widget->max_value = widget_json_get_double_default(obj, "max", 90.0);
    widget->window = widget_json_get_double_default(obj, "window", 40.0);
    widget->major_step = widget_json_get_double_default(obj, "major_step", 10.0);
    widget->minor_step = widget_json_get_double_default(obj, "minor_step", 1.0);

    if (widget->window <= 0.0) {
        widget->window = 40.0;
    }
    if (widget->major_step <= 0.0) {
        widget->major_step = 10.0;
    }
    if (widget->minor_step <= 0.0) {
        widget->minor_step = 1.0;
    }
    if (widget->max_value < widget->min_value) {
        double tmp = widget->max_value;
        widget->max_value = widget->min_value;
        widget->min_value = tmp;
    }
    return widget;
}

static int is_major_tick(double value, double step)
{
    double nearest = round(value / step) * step;
    return fabs(value - nearest) < 0.0001;
}

void vertical_ruler_widget_draw(struct overlay_widget *widget, cairo_t *cr,
                                const struct overlay_render_context *ctx)
{
    double value = widget_template_number(ctx, widget->value_expr, 0.0);
    double w = widget_coord_value(widget->w > 0.0 ? widget->w : 86.0, ctx->frame_w);
    double h = widget_coord_value(widget->h > 0.0 ? widget->h : 0.44, ctx->frame_h);
    double x = ctx->frame_x + widget_coord_value(widget->x, ctx->frame_w);
    double y = ctx->frame_y + widget_coord_value(widget->y, ctx->frame_h);
    double left;
    double top;
    double center_y;
    double start;
    double end;
    double tick;
    char text[64];

    if (value < widget->min_value) {
        value = widget->min_value;
    } else if (value > widget->max_value) {
        value = widget->max_value;
    }

    widget_anchor_origin(widget->anchor, x, y, w, h, &left, &top);
    center_y = top + h / 2.0;
    start = floor((value - widget->window / 2.0) / widget->minor_step) *
            widget->minor_step;
    end = value + widget->window / 2.0;

    cairo_save(cr);
    cairo_set_source_rgba(cr, ctx->hud_r, ctx->hud_g, ctx->hud_b,
                          widget->alpha * 0.14);
    cairo_rectangle(cr, left, top, w, h);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, ctx->hud_r, ctx->hud_g, ctx->hud_b, widget->alpha);
    cairo_set_line_width(cr, 1.2);
    cairo_move_to(cr, left + w * 0.56, top);
    cairo_line_to(cr, left + w * 0.56, top + h);
    cairo_stroke(cr);

    for (tick = start; tick <= end + 0.0001; tick += widget->minor_step) {
        double rel;
        double ty;
        double tick_len;
        int major;

        if (tick < widget->min_value || tick > widget->max_value) {
            continue;
        }
        rel = tick - value;
        ty = center_y - (rel / widget->window) * h;
        if (ty < top || ty > top + h) {
            continue;
        }
        major = is_major_tick(tick, widget->major_step);
        tick_len = major ? w * 0.36 : w * 0.18;

        cairo_move_to(cr, left + w * 0.56 - tick_len, ty);
        cairo_line_to(cr, left + w * 0.56, ty);
        cairo_stroke(cr);

        if (major) {
            char label[32];
            snprintf(label, sizeof(label), "%.0f", tick);
            widget_draw_text(cr, ctx, label, left + w * 0.08, ty + 5.0,
                             widget->size > 0.0 ? widget->size : 13.0,
                             widget->alpha, WIDGET_ANCHOR_TOP_LEFT);
        }
    }

    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, left + w * 0.56, center_y);
    cairo_line_to(cr, left + w * 0.74, center_y - 8.0);
    cairo_line_to(cr, left + w * 0.74, center_y + 8.0);
    cairo_close_path(cr);
    cairo_stroke(cr);

    snprintf(text, sizeof(text), "%s%.2f%s",
             widget->label ? widget->label : "",
             value,
             widget->suffix ? widget->suffix : "");
    widget_draw_text(cr, ctx, text, left + w / 2.0, top + h + 18.0,
                     widget->size > 0.0 ? widget->size : 13.0,
                     widget->alpha, WIDGET_ANCHOR_CENTER);
    cairo_restore(cr);
}
