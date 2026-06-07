#include "overlay_widgets.h"

#include "widgets/compass/compass_widget.h"
#include "widgets/image/image_widget.h"
#include "widgets/readout/readout_widget.h"
#include "widgets/status/status_widget.h"
#include "widgets/vertical_ruler/vertical_ruler_widget.h"

#include <stdio.h>
#include <string.h>

GPtrArray *overlay_widgets_new(void)
{
    return g_ptr_array_new_with_free_func(overlay_widget_free);
}

static struct overlay_widget *parse_widget(struct json_object *obj)
{
    const char *type;

    if (json_object_get_type(obj) != json_type_object) {
        fprintf(stderr, "overlay: widget invalido ignorado\n");
        return NULL;
    }

    type = widget_json_get_string_default(obj, "type", NULL);
    if (!type) {
        fprintf(stderr, "overlay: widget sem type ignorado\n");
        return NULL;
    }

    if (strcmp(type, "compass") == 0) {
        return compass_widget_parse(obj);
    }
    if (strcmp(type, "readout") == 0) {
        return readout_widget_parse(obj);
    }
    if (strcmp(type, "status") == 0) {
        return status_widget_parse(obj);
    }
    if (strcmp(type, "image") == 0) {
        return image_widget_parse(obj);
    }
    if (strcmp(type, "vertical_ruler") == 0) {
        return vertical_ruler_widget_parse(obj);
    }
    fprintf(stderr, "overlay: widget type desconhecido ignorado: %s\n", type);
    return NULL;
}

void overlay_widgets_load(GPtrArray *widgets, struct json_object *array)
{
    size_t i;

    if (!array || json_object_get_type(array) != json_type_array) {
        return;
    }

    g_ptr_array_set_size(widgets, 0);
    for (i = 0; i < json_object_array_length(array); i++) {
        struct overlay_widget *widget =
            parse_widget(json_object_array_get_idx(array, i));
        if (widget) {
            g_ptr_array_add(widgets, widget);
        }
    }
}

static gint compare_z(gconstpointer a, gconstpointer b)
{
    const struct overlay_widget *wa = *(const struct overlay_widget * const *)a;
    const struct overlay_widget *wb = *(const struct overlay_widget * const *)b;
    return wa->z_index - wb->z_index;
}

void overlay_widgets_draw(GPtrArray *widgets, cairo_t *cr,
                          const struct overlay_render_context *ctx)
{
    GPtrArray *sorted;
    unsigned i;

    if (!widgets || widgets->len == 0) {
        return;
    }

    sorted = g_ptr_array_new();
    for (i = 0; i < widgets->len; i++) {
        struct overlay_widget *widget = g_ptr_array_index(widgets, i);
        if (widget->visible) {
            g_ptr_array_add(sorted, widget);
        }
    }
    g_ptr_array_sort(sorted, compare_z);

    for (i = 0; i < sorted->len; i++) {
        struct overlay_widget *widget = g_ptr_array_index(sorted, i);
        if (widget->type == WIDGET_COMPASS) {
            compass_widget_draw(widget, cr, ctx);
        } else if (widget->type == WIDGET_READOUT) {
            readout_widget_draw(widget, cr, ctx);
        } else if (widget->type == WIDGET_STATUS) {
            status_widget_draw(widget, cr, ctx);
        } else if (widget->type == WIDGET_IMAGE) {
            image_widget_draw(widget, cr, ctx);
        } else if (widget->type == WIDGET_VERTICAL_RULER) {
            vertical_ruler_widget_draw(widget, cr, ctx);
        }
    }

    g_ptr_array_free(sorted, TRUE);
}
