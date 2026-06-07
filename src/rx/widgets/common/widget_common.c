#include "widget_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void readout_item_free(gpointer data)
{
    struct readout_item *item = data;
    if (!item) {
        return;
    }
    free(item->label);
    free(item->value);
    free(item);
}

void overlay_widget_free(gpointer data)
{
    struct overlay_widget *widget = data;
    if (!widget) {
        return;
    }
    free(widget->id);
    free(widget->file);
    free(widget->text);
    free(widget->azimuth_expr);
    free(widget->rotation_expr);
    free(widget->value_expr);
    free(widget->label);
    free(widget->suffix);
    for (int i = 0; i < 8; i++) {
        free(widget->compass_labels[i]);
    }
    if (widget->items) {
        g_ptr_array_free(widget->items, TRUE);
    }
    if (widget->pixbuf) {
        g_object_unref(widget->pixbuf);
    }
    free(widget);
}

int widget_json_get_obj(struct json_object *obj, const char *key,
                 struct json_object **out)
{
    return json_object_object_get_ex(obj, key, out);
}

const char *widget_json_get_string_default(struct json_object *obj, const char *key,
                                    const char *fallback)
{
    struct json_object *value;
    if (!widget_json_get_obj(obj, key, &value) ||
        json_object_get_type(value) != json_type_string) {
        return fallback;
    }
    return json_object_get_string(value);
}

char *widget_dup_json_string(struct json_object *obj, const char *key,
                      const char *fallback)
{
    const char *value = widget_json_get_string_default(obj, key, fallback);
    return value ? strdup(value) : NULL;
}

double widget_json_get_double_default(struct json_object *obj, const char *key,
                               double fallback)
{
    struct json_object *value;
    if (!widget_json_get_obj(obj, key, &value)) {
        return fallback;
    }
    if (json_object_get_type(value) != json_type_double &&
        json_object_get_type(value) != json_type_int) {
        return fallback;
    }
    return json_object_get_double(value);
}

int widget_json_get_int_default(struct json_object *obj, const char *key,
                         int fallback)
{
    struct json_object *value;
    if (!widget_json_get_obj(obj, key, &value) ||
        json_object_get_type(value) != json_type_int) {
        return fallback;
    }
    return json_object_get_int(value);
}

int widget_json_get_bool_default(struct json_object *obj, const char *key,
                          int fallback)
{
    struct json_object *value;
    if (!widget_json_get_obj(obj, key, &value) ||
        json_object_get_type(value) != json_type_boolean) {
        return fallback;
    }
    return json_object_get_boolean(value);
}

static enum widget_anchor parse_anchor(const char *anchor)
{
    if (strcmp(anchor, "top-right") == 0) {
        return WIDGET_ANCHOR_TOP_RIGHT;
    }
    if (strcmp(anchor, "top-center") == 0) {
        return WIDGET_ANCHOR_TOP_CENTER;
    }
    if (strcmp(anchor, "bottom-left") == 0) {
        return WIDGET_ANCHOR_BOTTOM_LEFT;
    }
    if (strcmp(anchor, "bottom-right") == 0) {
        return WIDGET_ANCHOR_BOTTOM_RIGHT;
    }
    if (strcmp(anchor, "center") == 0) {
        return WIDGET_ANCHOR_CENTER;
    }
    return WIDGET_ANCHOR_TOP_LEFT;
}

struct overlay_widget *widget_base(struct json_object *obj,
                                   enum widget_type type)
{
    struct overlay_widget *widget = calloc(1, sizeof(*widget));
    if (!widget) {
        return NULL;
    }
    widget->type = type;
    widget->id = widget_dup_json_string(obj, "id", NULL);
    widget->x = widget_json_get_double_default(obj, "x", 0.0);
    widget->y = widget_json_get_double_default(obj, "y", 0.0);
    widget->w = widget_json_get_double_default(obj, "w", 0.0);
    widget->h = widget_json_get_double_default(obj, "h", 0.0);
    widget->alpha = widget_json_get_double_default(obj, "alpha", 1.0);
    widget->size = widget_json_get_double_default(obj, "size", 16.0);
    widget->width = widget_json_get_double_default(obj, "width", 2.0);
    widget->visible = widget_json_get_bool_default(obj, "visible", 1);
    widget->z_index = widget_json_get_int_default(obj, "z_index", 0);
    widget->anchor = parse_anchor(widget_json_get_string_default(obj, "anchor", "top-left"));
    if (widget->alpha < 0.0) {
        widget->alpha = 0.0;
    } else if (widget->alpha > 1.0) {
        widget->alpha = 1.0;
    }
    return widget;
}

int widget_valid_image_path(const char *file)
{
    return file && file[0] && file[0] != '/' && !strstr(file, "..");
}

void widget_anchor_origin(enum widget_anchor anchor, double x, double y,
                   double w, double h, double *out_x, double *out_y)
{
    *out_x = x;
    *out_y = y;
    if (anchor == WIDGET_ANCHOR_TOP_RIGHT) {
        *out_x = x - w;
    } else if (anchor == WIDGET_ANCHOR_TOP_CENTER) {
        *out_x = x - w / 2.0;
    } else if (anchor == WIDGET_ANCHOR_BOTTOM_LEFT) {
        *out_y = y - h;
    } else if (anchor == WIDGET_ANCHOR_BOTTOM_RIGHT) {
        *out_x = x - w;
        *out_y = y - h;
    } else if (anchor == WIDGET_ANCHOR_CENTER) {
        *out_x = x - w / 2.0;
        *out_y = y - h / 2.0;
    }
}

double widget_coord_value(double value, double extent)
{
    if (value >= 0.0 && value <= 1.0) {
        return value * extent;
    }
    return value;
}

char *widget_expand_template(const struct overlay_render_context *ctx,
                      const char *text)
{
    GString *out = g_string_new("");
    const char *p = text ? text : "";

    while (*p) {
        const char *open = strchr(p, '{');
        const char *close;
        char *key;
        const char *value;

        if (!open) {
            g_string_append(out, p);
            break;
        }
        g_string_append_len(out, p, open - p);
        close = strchr(open + 1, '}');
        if (!close) {
            g_string_append(out, open);
            break;
        }
        key = g_strndup(open + 1, close - open - 1);
        value = g_hash_table_lookup(ctx->vars, key);
        if (value) {
            g_string_append(out, value);
        }
        g_free(key);
        p = close + 1;
    }

    return g_string_free(out, FALSE);
}

double widget_template_number(const struct overlay_render_context *ctx,
                       const char *expr, double fallback)
{
    char *expanded;
    char *end = NULL;
    double value;

    if (!expr) {
        return fallback;
    }
    expanded = widget_expand_template(ctx, expr);
    value = g_ascii_strtod(expanded, &end);
    if (!expanded[0] || !end || *end != '\0') {
        value = fallback;
    }
    g_free(expanded);
    return value;
}

int widget_template_number_found(const struct overlay_render_context *ctx,
                                 const char *expr, double *out)
{
    char *expanded;
    char *end = NULL;
    double value;

    if (!expr) {
        return 0;
    }
    expanded = widget_expand_template(ctx, expr);
    value = g_ascii_strtod(expanded, &end);
    if (!expanded[0] || !end || *end != '\0') {
        g_free(expanded);
        return 0;
    }
    g_free(expanded);
    *out = value;
    return 1;
}

void widget_draw_text(cairo_t *cr, const struct overlay_render_context *ctx,
               const char *text, double x, double y, double size,
               double alpha, enum widget_anchor anchor)
{
    cairo_text_extents_t ext;
    double left;
    double top;

    cairo_save(cr);
    cairo_select_font_face(cr, ctx->hud_font ? ctx->hud_font : "Monospace",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_text_extents(cr, text, &ext);
    widget_anchor_origin(anchor, x, y, ext.width, ext.height, &left, &top);
    cairo_set_source_rgba(cr, ctx->hud_r, ctx->hud_g, ctx->hud_b, alpha);
    cairo_move_to(cr, left - ext.x_bearing, top - ext.y_bearing);
    cairo_show_text(cr, text);
    cairo_restore(cr);
}

void load_widget_pixbuf(const struct overlay_render_context *ctx,
                        struct overlay_widget *widget)
{
    char *path;
    GError *err = NULL;

    if (widget->pixbuf || widget->pixbuf_failed || !widget_valid_image_path(widget->file)) {
        return;
    }
    path = g_build_filename(ctx->assets_dir ? ctx->assets_dir : ".",
                            widget->file, NULL);
    widget->pixbuf = gdk_pixbuf_new_from_file(path, &err);
    if (!widget->pixbuf) {
        fprintf(stderr, "overlay: widget image ausente ou invalida %s: %s\n",
                path, err ? err->message : "erro desconhecido");
        g_clear_error(&err);
        widget->pixbuf_failed = 1;
    }
    g_free(path);
}
