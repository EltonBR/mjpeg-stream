#include "overlay_widgets.h"

#include "widgets/compass/compass_widget.h"
#include "widgets/image/image_widget.h"
#include "widgets/readout/readout_widget.h"
#include "widgets/status/status_widget.h"
#include "widgets/vertical_ruler/vertical_ruler_widget.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

GPtrArray *overlay_widgets_new(void)
{
    return g_ptr_array_new_with_free_func(overlay_widget_free);
}

static struct overlay_widget *parse_widget(struct json_object *obj)
{
    const char *type;
    const char *unit_keys[] = {"x_unit", "y_unit", "w_unit", "h_unit"};
    const char *number_keys[] = {"x", "y", "w", "h", "alpha", "size", "z_index",
                                 "min", "max", "window", "major_step", "minor_step", "width"};
    size_t i;

    if (json_object_get_type(obj) != json_type_object) {
        fprintf(stderr, "overlay: widget invalido ignorado\n");
        return NULL;
    }

    {
        const char *string_keys[] = {"id", "anchor", "file", "text", "value",
                                     "azimuth", "rotation", "label", "suffix"};
        const char *bool_keys[] = {"visible", "flip_horizontal", "flip_vertical"};
        for (i = 0; i < G_N_ELEMENTS(string_keys); i++) {
            struct json_object *value;
            if (json_object_object_get_ex(obj, string_keys[i], &value) &&
                json_object_get_type(value) != json_type_string) return NULL;
        }
        for (i = 0; i < G_N_ELEMENTS(bool_keys); i++) {
            struct json_object *value;
            if (json_object_object_get_ex(obj, bool_keys[i], &value) &&
                json_object_get_type(value) != json_type_boolean) return NULL;
        }
        {
            struct json_object *value;
            if (json_object_object_get_ex(obj, "z_index", &value) &&
                json_object_get_type(value) != json_type_int) return NULL;
            if (json_object_object_get_ex(obj, "anchor", &value)) {
                const char *anchors[] = {"top-left", "top-center", "top-right",
                                         "bottom-left", "bottom-right", "center"};
                size_t j;
                if (json_object_get_type(value) != json_type_string) return NULL;
                for (j = 0; j < G_N_ELEMENTS(anchors) &&
                     strcmp(json_object_get_string(value), anchors[j]) != 0; j++);
                if (j == G_N_ELEMENTS(anchors)) return NULL;
            }
        }
        {
            struct json_object *items;
            if (json_object_object_get_ex(obj, "items", &items)) {
                size_t j;
                if (json_object_get_type(items) != json_type_array) return NULL;
                for (j = 0; j < json_object_array_length(items); j++) {
                    struct json_object *item = json_object_array_get_idx(items, j);
                    struct json_object *label, *value;
                    if (json_object_get_type(item) != json_type_object ||
                        !json_object_object_get_ex(item, "label", &label) ||
                        !json_object_object_get_ex(item, "value", &value) ||
                        json_object_get_type(label) != json_type_string ||
                        json_object_get_type(value) != json_type_string) return NULL;
                }
            }
        }
        {
            struct json_object *labels;
            if (json_object_object_get_ex(obj, "labels", &labels)) {
                size_t j;
                if (json_object_get_type(labels) != json_type_array ||
                    json_object_array_length(labels) != 8) return NULL;
                for (j = 0; j < 8; j++) {
                    if (json_object_get_type(json_object_array_get_idx(labels, j)) !=
                        json_type_string) return NULL;
                }
            }
        }
    }

    for (i = 0; i < G_N_ELEMENTS(unit_keys); i++) {
        struct json_object *value;
        if (json_object_object_get_ex(obj, unit_keys[i], &value) &&
            (json_object_get_type(value) != json_type_string ||
             (strcmp(json_object_get_string(value), "relative") != 0 &&
              strcmp(json_object_get_string(value), "px") != 0))) return NULL;
    }
    for (i = 0; i < G_N_ELEMENTS(number_keys); i++) {
        struct json_object *value;
        if (json_object_object_get_ex(obj, number_keys[i], &value)) {
            enum json_type kind = json_object_get_type(value);
            double number;
            if (kind != json_type_int && kind != json_type_double) return NULL;
            number = json_object_get_double(value);
            if (!isfinite(number) || ((i == 2 || i == 3 || i == 5) && number < 0) ||
                (i == 4 && (number < 0 || number > 1)) ||
                (i >= 9 && number <= 0) || (i == 12 && number <= 0)) return NULL;
        }
    }
    {
        struct json_object *z_index;
        if (json_object_object_get_ex(obj, "z_index", &z_index) &&
            json_object_get_type(z_index) != json_type_int) return NULL;
    }
    {
        struct json_object *minimum, *maximum;
        if (json_object_object_get_ex(obj, "min", &minimum) &&
            json_object_object_get_ex(obj, "max", &maximum) &&
            json_object_get_double(minimum) >= json_object_get_double(maximum)) return NULL;
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
    if (strcmp(type, "horizontal_ruler") == 0) {
        return horizontal_ruler_widget_parse(obj);
    }
    fprintf(stderr, "overlay: widget type desconhecido ignorado: %s\n", type);
    return NULL;
}

int overlay_widgets_load(GPtrArray *widgets, struct json_object *array)
{
    size_t i;

    if (!array || json_object_get_type(array) != json_type_array) {
        return array ? -1 : 0;
    }

    {
        GPtrArray *parsed = overlay_widgets_new();
    for (i = 0; i < json_object_array_length(array); i++) {
        struct overlay_widget *widget =
            parse_widget(json_object_array_get_idx(array, i));
        if (!widget) {
            g_ptr_array_free(parsed, TRUE);
            return -1;
        }
        g_ptr_array_add(parsed, widget);
    }
        g_ptr_array_set_size(widgets, 0);
        for (i = 0; i < parsed->len; i++)
            g_ptr_array_add(widgets, g_ptr_array_index(parsed, i));
        g_ptr_array_set_free_func(parsed, NULL);
        g_ptr_array_free(parsed, TRUE);
    }
    return 0;
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
        overlay_widget_draw_one(widget, cr, ctx);
    }

    g_ptr_array_free(sorted, TRUE);
}

void overlay_widget_draw_one(struct overlay_widget *widget, cairo_t *cr,
                             const struct overlay_render_context *ctx)
{
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
    } else if (widget->type == WIDGET_HORIZONTAL_RULER) {
        horizontal_ruler_widget_draw(widget, cr, ctx);
    }
}
