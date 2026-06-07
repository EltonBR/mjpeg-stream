#include "readout_widget.h"

#include <stdio.h>
#include <stdlib.h>

struct overlay_widget *readout_widget_parse(struct json_object *obj)
{
    struct json_object *items;
    struct overlay_widget *widget = widget_base(obj, WIDGET_READOUT);
    size_t i;

    if (!widget) {
        return NULL;
    }
    widget->items = g_ptr_array_new_with_free_func(readout_item_free);
    if (widget_json_get_obj(obj, "items", &items) &&
        json_object_get_type(items) == json_type_array) {
        for (i = 0; i < json_object_array_length(items); i++) {
            struct json_object *item_obj = json_object_array_get_idx(items, i);
            struct readout_item *item;
            if (json_object_get_type(item_obj) != json_type_object) {
                continue;
            }
            item = calloc(1, sizeof(*item));
            if (!item) {
                continue;
            }
            item->label = widget_dup_json_string(item_obj, "label", "");
            item->value = widget_dup_json_string(item_obj, "value", "");
            g_ptr_array_add(widget->items, item);
        }
    }
    return widget;
}

void readout_widget_draw(struct overlay_widget *widget, cairo_t *cr,
                         const struct overlay_render_context *ctx)
{
    double x = ctx->frame_x + widget_coord_value(widget->x, ctx->frame_w);
    double y = ctx->frame_y + widget_coord_value(widget->y, ctx->frame_h);
    unsigned i;

    if (!widget->items) {
        return;
    }
    for (i = 0; i < widget->items->len; i++) {
        struct readout_item *item = g_ptr_array_index(widget->items, i);
        char *value = widget_expand_template(ctx, item->value);
        char text[256];
        snprintf(text, sizeof(text), "%s%s", item->label ? item->label : "", value);
        widget_draw_text(cr, ctx, text, x, y + i * (widget->size + 8.0),
                  widget->size, widget->alpha, widget->anchor);
        g_free(value);
    }
}
