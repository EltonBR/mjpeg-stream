#include "compass_widget.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct overlay_widget *compass_widget_parse(struct json_object *obj)
{
    struct overlay_widget *widget = widget_base(obj, WIDGET_COMPASS);
    struct json_object *labels;
    size_t i;

    if (widget) {
        widget->azimuth_expr = widget_dup_json_string(obj, "azimuth", "{azimuth}");
        if (widget_json_get_obj(obj, "labels", &labels) &&
            json_object_get_type(labels) == json_type_array &&
            json_object_array_length(labels) == 8) {
            for (i = 0; i < 8; i++) {
                struct json_object *label = json_object_array_get_idx(labels, i);
                if (json_object_get_type(label) == json_type_string) {
                    widget->compass_labels[i] = strdup(json_object_get_string(label));
                }
            }
        }
    }
    return widget;
}

static const char *cardinal_for(const struct overlay_widget *widget, int heading)
{
    static const char *names[] = {
        "N", "NE", "E", "SE", "S", "SW", "W", "NW"
    };
    int idx = (int)floor((heading + 22.5) / 45.0) & 7;
    return widget->compass_labels[idx] ? widget->compass_labels[idx] : names[idx];
}

void compass_widget_draw(struct overlay_widget *widget, cairo_t *cr,
                         const struct overlay_render_context *ctx)
{
    double azimuth = 0.0;
    double w = widget_coord_value(widget->w > 0.0 ? widget->w : 0.48, ctx->frame_w);
    double h = widget_coord_value(widget->h > 0.0 ? widget->h : 58.0, ctx->frame_h);
    double x = ctx->frame_x + widget_coord_value(widget->x, ctx->frame_w);
    double y = ctx->frame_y + widget_coord_value(widget->y, ctx->frame_h);
    double left;
    double top;
    int base;
    int tick;
    char heading_text[64];

    if (!widget_template_number_found(ctx, widget->azimuth_expr, &azimuth) &&
        !widget_template_number_found(ctx, "{azimuth}", &azimuth)) {
        (void)widget_template_number_found(ctx, "{heading}", &azimuth);
    }

    while (azimuth < 0.0) {
        azimuth += 360.0;
    }
    while (azimuth >= 360.0) {
        azimuth -= 360.0;
    }
    widget_anchor_origin(widget->anchor, x, y, w, h, &left, &top);
    base = (int)floor(azimuth / 5.0) * 5;

    cairo_save(cr);
    cairo_set_source_rgba(cr, ctx->hud_r, ctx->hud_g, ctx->hud_b, widget->alpha * 0.18);
    cairo_rectangle(cr, left, top + h * 0.30, w, h * 0.42);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, ctx->hud_r, ctx->hud_g, ctx->hud_b, widget->alpha);
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, left, top + h * 0.72);
    cairo_line_to(cr, left + w, top + h * 0.72);
    cairo_stroke(cr);

    for (tick = base - 45; tick <= base + 45; tick += 5) {
        int normalized = ((tick % 360) + 360) % 360;
        double rel = tick - azimuth;
        double tx = left + w / 2.0 + (rel / 90.0) * w;
        double tick_h = normalized % 30 == 0 ? h * 0.28 :
                        normalized % 10 == 0 ? h * 0.20 : h * 0.12;

        if (tx < left || tx > left + w) {
            continue;
        }
        cairo_move_to(cr, tx, top + h * 0.70);
        cairo_line_to(cr, tx, top + h * 0.70 - tick_h);
        cairo_stroke(cr);
        if (normalized % 30 == 0) {
            char label[32];
            if (normalized % 90 == 0) {
                snprintf(label, sizeof(label), "%s", cardinal_for(widget, normalized));
            } else {
                snprintf(label, sizeof(label), "%03d", normalized);
            }
            widget_draw_text(cr, ctx, label, tx, top + h * 0.16,
                      widget->size > 0.0 ? widget->size : 14.0,
                      widget->alpha, WIDGET_ANCHOR_CENTER);
        }
    }

    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, left + w / 2.0, top + h * 0.78);
    cairo_line_to(cr, left + w / 2.0 - 8.0, top + h);
    cairo_line_to(cr, left + w / 2.0 + 8.0, top + h);
    cairo_close_path(cr);
    cairo_stroke(cr);

    widget_snprintf_dot(heading_text, sizeof(heading_text), "AZ %06.2f %s",
             azimuth, cardinal_for(widget, (int)round(azimuth) % 360));
    widget_draw_text(cr, ctx, heading_text, left + w / 2.0, top + h + 18.0,
              widget->size > 0.0 ? widget->size : 14.0,
              widget->alpha, WIDGET_ANCHOR_CENTER);
    cairo_restore(cr);
}
