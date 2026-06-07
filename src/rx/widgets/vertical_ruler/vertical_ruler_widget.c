#include "vertical_ruler_widget.h"

#include <math.h>
#include <stdio.h>

static struct overlay_widget *ruler_widget_parse(struct json_object *obj,
                                                 enum widget_type type)
{
    struct overlay_widget *widget = widget_base(obj, type);
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
    widget->flip_horizontal = widget_json_get_bool_default(obj, "flip_horizontal", 0);
    widget->flip_vertical = widget_json_get_bool_default(obj, "flip_vertical", 0);

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

struct overlay_widget *vertical_ruler_widget_parse(struct json_object *obj)
{
    return ruler_widget_parse(obj, WIDGET_VERTICAL_RULER);
}

struct overlay_widget *horizontal_ruler_widget_parse(struct json_object *obj)
{
    return ruler_widget_parse(obj, WIDGET_HORIZONTAL_RULER);
}

static int is_major_tick(double value, double step)
{
    double nearest = round(value / step) * step;
    return fabs(value - nearest) < 0.0001;
}

static double clamped_value(struct overlay_widget *widget, double raw_value)
{
    double value = raw_value;

    if (value < widget->min_value) {
        value = widget->min_value;
    } else if (value > widget->max_value) {
        value = widget->max_value;
    }
    return value;
}

static void draw_value_label(struct overlay_widget *widget, cairo_t *cr,
                             const struct overlay_render_context *ctx,
                             double raw_value, double x, double y)
{
    char text[64];

    snprintf(text, sizeof(text), "%s%.2f%s",
             widget->label ? widget->label : "",
             raw_value,
             widget->suffix ? widget->suffix : "");
    widget_draw_text(cr, ctx, text, x, y,
                     widget->size > 0.0 ? widget->size : 13.0,
                     widget->alpha, WIDGET_ANCHOR_CENTER);
}

void vertical_ruler_widget_draw(struct overlay_widget *widget, cairo_t *cr,
                                const struct overlay_render_context *ctx)
{
    double raw_value = widget_template_number(ctx, widget->value_expr, 0.0);
    double value = clamped_value(widget, raw_value);
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
    double axis_x;
    double label_x;
    enum widget_anchor label_anchor;

    widget_anchor_origin(widget->anchor, x, y, w, h, &left, &top);
    center_y = top + h / 2.0;
    axis_x = left + w * (widget->flip_horizontal ? 0.44 : 0.56);
    label_x = left + w * (widget->flip_horizontal ? 0.84 : 0.16);
    label_anchor = widget->flip_horizontal ?
                   WIDGET_ANCHOR_TOP_LEFT : WIDGET_ANCHOR_TOP_RIGHT;
    start = floor((value - widget->window / 2.0) / widget->minor_step) *
            widget->minor_step;
    end = value + widget->window / 2.0;

    cairo_save(cr);
    cairo_set_source_rgba(cr, ctx->hud_r, ctx->hud_g, ctx->hud_b,
                          0);
    cairo_rectangle(cr, left, top, w, h);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, ctx->hud_r, ctx->hud_g, ctx->hud_b, widget->alpha);
    cairo_set_line_width(cr, 1.2);
    cairo_move_to(cr, axis_x, top);
    cairo_line_to(cr, axis_x, top + h);
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

        cairo_move_to(cr, axis_x, ty);
        cairo_line_to(cr, axis_x + (widget->flip_horizontal ? tick_len : -tick_len), ty);
        cairo_stroke(cr);

        if (major) {
            char label[32];
            snprintf(label, sizeof(label), "%.0f", tick);
            widget_draw_text(cr, ctx, label, label_x, ty + 5.0,
                             widget->size > 0.0 ? widget->size : 13.0,
                             widget->alpha, label_anchor);
        }
    }

    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, axis_x, center_y);
    cairo_line_to(cr, axis_x + (widget->flip_horizontal ? -w * 0.18 : w * 0.18),
                  center_y - 8.0);
    cairo_line_to(cr, axis_x + (widget->flip_horizontal ? -w * 0.18 : w * 0.18),
                  center_y + 8.0);
    cairo_close_path(cr);
    cairo_stroke(cr);

    draw_value_label(widget, cr, ctx, raw_value, left + w / 2.0, top + h + 18.0);
    cairo_restore(cr);
}

void horizontal_ruler_widget_draw(struct overlay_widget *widget, cairo_t *cr,
                                  const struct overlay_render_context *ctx)
{
    double raw_value = widget_template_number(ctx, widget->value_expr, 0.0);
    double value = clamped_value(widget, raw_value);
    double w = widget_coord_value(widget->w > 0.0 ? widget->w : 0.46, ctx->frame_w);
    double h = widget_coord_value(widget->h > 0.0 ? widget->h : 70.0, ctx->frame_h);
    double x = ctx->frame_x + widget_coord_value(widget->x, ctx->frame_w);
    double y = ctx->frame_y + widget_coord_value(widget->y, ctx->frame_h);
    double left;
    double top;
    double center_x;
    double axis_y;
    double label_y;
    double start;
    double end;
    double tick;
    double direction = widget->flip_horizontal ? -1.0 : 1.0;

    widget_anchor_origin(widget->anchor, x, y, w, h, &left, &top);
    center_x = left + w / 2.0;
    axis_y = top + h * (widget->flip_vertical ? 0.44 : 0.56);
    label_y = axis_y + (widget->flip_vertical ? -h * 0.42 : h * 0.42);
    start = floor((value - widget->window / 2.0) / widget->minor_step) *
            widget->minor_step;
    end = value + widget->window / 2.0;

    cairo_save(cr);
    cairo_set_source_rgba(cr, ctx->hud_r, ctx->hud_g, ctx->hud_b, 0);
    cairo_rectangle(cr, left, top, w, h);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, ctx->hud_r, ctx->hud_g, ctx->hud_b, widget->alpha);
    cairo_set_line_width(cr, 1.2);
    cairo_move_to(cr, left, axis_y);
    cairo_line_to(cr, left + w, axis_y);
    cairo_stroke(cr);

    for (tick = start; tick <= end + 0.0001; tick += widget->minor_step) {
        double rel;
        double tx;
        double tick_len;
        int major;

        if (tick < widget->min_value || tick > widget->max_value) {
            continue;
        }
        rel = tick - value;
        tx = center_x + direction * (rel / widget->window) * w;
        if (tx < left || tx > left + w) {
            continue;
        }
        major = is_major_tick(tick, widget->major_step);
        tick_len = major ? h * 0.34 : h * 0.18;

        cairo_move_to(cr, tx, axis_y);
        cairo_line_to(cr, tx, axis_y + (widget->flip_vertical ? -tick_len : tick_len));
        cairo_stroke(cr);

        if (major) {
            char label[32];
            snprintf(label, sizeof(label), "%.0f", tick);
            widget_draw_text(cr, ctx, label, tx, label_y,
                             widget->size > 0.0 ? widget->size : 13.0,
                             widget->alpha, WIDGET_ANCHOR_CENTER);
        }
    }

    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, center_x, axis_y);
    cairo_line_to(cr, center_x - 8.0,
                  axis_y + (widget->flip_vertical ? h * 0.18 : -h * 0.18));
    cairo_line_to(cr, center_x + 8.0,
                  axis_y + (widget->flip_vertical ? h * 0.18 : -h * 0.18));
    cairo_close_path(cr);
    cairo_stroke(cr);

    draw_value_label(widget, cr, ctx, raw_value, left + w / 2.0,
                     axis_y + (widget->flip_vertical ? h * 0.26 : -h * 0.26));
    cairo_restore(cr);
}
