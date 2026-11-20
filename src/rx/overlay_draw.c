#include "overlay.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int valid_image_path(const char *file)
{
    if (!file || !file[0] || file[0] == '/' || strstr(file, "..")) {
        return 0;
    }
    return 1;
}

static double coord_value(double value, double extent)
{
    if (value >= 0.0 && value <= 1.0) {
        return value * extent;
    }
    return value;
}

static char *expand_template_locked(GHashTable *vars, const char *text)
{
    GString *out = g_string_new("");
    const char *p = text;

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
        value = g_hash_table_lookup(vars, key);
        if (value) {
            g_string_append(out, value);
        }
        g_free(key);
        p = close + 1;
    }

    return g_string_free(out, FALSE);
}

static double rotation_value_locked(struct overlay_element *el,
                                    GHashTable *vars)
{
    double value;
    char *expanded;
    char *end = NULL;

    if (!el->rotation_expr) {
        return el->rotation;
    }
    expanded = expand_template_locked(vars, el->rotation_expr);
    value = g_ascii_strtod(expanded, &end);
    if (!expanded[0] || !end || *end != '\0') {
        value = 0.0;
    }
    g_free(expanded);
    return value;
}

static void anchored_top_left(enum overlay_anchor anchor, double x, double y,
                              double w, double h, double *out_x,
                              double *out_y)
{
    *out_x = x;
    *out_y = y;
    if (anchor == OVERLAY_ANCHOR_TOP_RIGHT) {
        *out_x = x - w;
    } else if (anchor == OVERLAY_ANCHOR_BOTTOM_LEFT) {
        *out_y = y - h;
    } else if (anchor == OVERLAY_ANCHOR_BOTTOM_RIGHT) {
        *out_x = x - w;
        *out_y = y - h;
    } else if (anchor == OVERLAY_ANCHOR_CENTER) {
        *out_x = x - w / 2.0;
        *out_y = y - h / 2.0;
    }
}

static void load_pixbuf_locked(struct overlay_state *overlay,
                               struct overlay_element *el)
{
    char *path;
    GError *err = NULL;

    if (el->pixbuf || el->pixbuf_failed || !valid_image_path(el->file)) {
        return;
    }

    path = g_build_filename(overlay->assets_dir ? overlay->assets_dir : ".",
                            el->file, NULL);
    el->pixbuf = gdk_pixbuf_new_from_file(path, &err);
    if (!el->pixbuf) {
        fprintf(stderr, "overlay: imagem ausente ou invalida %s: %s\n",
                path, err ? err->message : "erro desconhecido");
        g_clear_error(&err);
        el->pixbuf_failed = 1;
    }
    g_free(path);
}

static void draw_image_locked(struct overlay_state *overlay,
                              struct overlay_element *el, cairo_t *cr,
                              double fx, double fy, double fw, double fh,
                              GHashTable *vars)
{
    double x;
    double y;
    double w;
    double h;
    double left;
    double top;
    double angle;

    load_pixbuf_locked(overlay, el);
    if (!el->pixbuf) {
        return;
    }

    x = fx + coord_value(el->x, fw);
    y = fy + coord_value(el->y, fh);
    w = coord_value(el->w, fw);
    h = coord_value(el->h, fh);
    if (w <= 0.0) {
        w = gdk_pixbuf_get_width(el->pixbuf);
    }
    if (h <= 0.0) {
        h = gdk_pixbuf_get_height(el->pixbuf);
    }
    anchored_top_left(el->anchor, x, y, w, h, &left, &top);
    angle = rotation_value_locked(el, vars) * G_PI / 180.0;

    cairo_save(cr);
    cairo_translate(cr, left + w / 2.0, top + h / 2.0);
    cairo_rotate(cr, angle);
    cairo_scale(cr, w / gdk_pixbuf_get_width(el->pixbuf),
                h / gdk_pixbuf_get_height(el->pixbuf));
    gdk_cairo_set_source_pixbuf(cr, el->pixbuf,
                                -gdk_pixbuf_get_width(el->pixbuf) / 2.0,
                                -gdk_pixbuf_get_height(el->pixbuf) / 2.0);
    cairo_paint_with_alpha(cr, el->alpha);
    cairo_restore(cr);
}

static void draw_label_locked(struct overlay_state *overlay,
                              struct overlay_element *el, cairo_t *cr,
                              double fx, double fy, double fw, double fh,
                              GHashTable *vars)
{
    char *text = expand_template_locked(vars, el->text);
    cairo_text_extents_t ext;
    double x = fx + coord_value(el->x, fw);
    double y = fy + coord_value(el->y, fh);
    double left;
    double top;

    cairo_save(cr);
    cairo_select_font_face(cr, overlay->hud_font ? overlay->hud_font : "Monospace",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, el->width);
    cairo_text_extents(cr, text, &ext);
    anchored_top_left(el->anchor, x, y, ext.width, ext.height, &left, &top);
    cairo_set_source_rgba(cr, overlay->hud_r, overlay->hud_g, overlay->hud_b,
                          el->alpha);
    cairo_move_to(cr, left - ext.x_bearing, top - ext.y_bearing);
    cairo_show_text(cr, text);
    cairo_restore(cr);
    g_free(text);
}

static void draw_line_locked(struct overlay_state *overlay,
                             struct overlay_element *el, cairo_t *cr,
                             double fx, double fy, double fw, double fh,
                             GHashTable *vars)
{
    double x1 = fx + coord_value(el->x1, fw);
    double y1 = fy + coord_value(el->y1, fh);
    double x2 = fx + coord_value(el->x2, fw);
    double y2 = fy + coord_value(el->y2, fh);
    double cx = (x1 + x2) / 2.0;
    double cy = (y1 + y2) / 2.0;
    double angle = rotation_value_locked(el, vars) * G_PI / 180.0;
    double cos_a = cos(angle);
    double sin_a = sin(angle);
    double dx1 = x1 - cx;
    double dy1 = y1 - cy;
    double dx2 = x2 - cx;
    double dy2 = y2 - cy;
    double rx1 = cx + dx1 * cos_a - dy1 * sin_a;
    double ry1 = cy + dx1 * sin_a + dy1 * cos_a;
    double rx2 = cx + dx2 * cos_a - dy2 * sin_a;
    double ry2 = cy + dx2 * sin_a + dy2 * cos_a;

    cairo_save(cr);
    cairo_set_source_rgba(cr, overlay->hud_r, overlay->hud_g, overlay->hud_b,
                          el->alpha);
    cairo_set_line_width(cr, el->width > 0.0 ? el->width : 1.0);
    cairo_move_to(cr, rx1, ry1);
    cairo_line_to(cr, rx2, ry2);
    cairo_stroke(cr);
    cairo_restore(cr);
}

struct draw_item { int z; int widget; void *item; };
static gint compare_z(gconstpointer a, gconstpointer b)
{
    const struct draw_item *ea = *(const struct draw_item * const *)a;
    const struct draw_item *eb = *(const struct draw_item * const *)b;
    return (ea->z > eb->z) - (ea->z < eb->z);
}

void overlay_draw_with_store(struct overlay_state *overlay,
                             TelemetryStore *store, cairo_t *cr,
                             double frame_x, double frame_y,
                             double frame_w, double frame_h)
{
    GPtrArray *sorted;
    unsigned i;
    struct overlay_render_context ctx;
    GHashTable *snapshot = store ? telemetry_store_snapshot(store) : NULL;
    GHashTable *vars;

    g_mutex_lock(&overlay->lock);
    if (!overlay->enabled) {
        g_mutex_unlock(&overlay->lock);
        if (snapshot) {
            g_hash_table_destroy(snapshot);
        }
        return;
    }
    vars = snapshot ? snapshot : overlay->vars;
    ctx.frame_x = frame_x;
    ctx.frame_y = frame_y;
    ctx.frame_w = frame_w;
    ctx.frame_h = frame_h;
    ctx.hud_r = overlay->hud_r;
    ctx.hud_g = overlay->hud_g;
    ctx.hud_b = overlay->hud_b;
    ctx.hud_font = overlay->hud_font ? overlay->hud_font : "Monospace";
    ctx.assets_dir = overlay->assets_dir;
    ctx.vars = vars;

    sorted = g_ptr_array_new_with_free_func(g_free);
    if (overlay->dim_alpha > 0.0) {
        cairo_save(cr);
        cairo_set_source_rgba(cr, overlay->dim_r, overlay->dim_g,
                              overlay->dim_b, overlay->dim_alpha);
        cairo_rectangle(cr, frame_x, frame_y, frame_w, frame_h);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    for (i = 0; i < overlay->elements->len; i++) {
        struct overlay_element *el = g_ptr_array_index(overlay->elements, i);
        if (el->visible) {
            struct draw_item *item = g_new(struct draw_item, 1);
            *item = (struct draw_item){el->z_index, 0, el};
            g_ptr_array_add(sorted, item);
        }
    }
    for (i = 0; i < overlay->widgets->len; i++) {
        struct overlay_widget *w = g_ptr_array_index(overlay->widgets, i);
        if (w->visible) {
            struct draw_item *item = g_new(struct draw_item, 1);
            *item = (struct draw_item){w->z_index, 1, w};
            g_ptr_array_add(sorted, item);
        }
    }
    g_ptr_array_sort(sorted, compare_z);

    for (i = 0; i < sorted->len; i++) {
        struct draw_item *item = g_ptr_array_index(sorted, i);
        struct overlay_element *el = item->item;
        if (item->widget) {
            overlay_widget_draw_one(item->item, cr, &ctx);
        } else if (el->type == OVERLAY_IMAGE) {
            draw_image_locked(overlay, el, cr, frame_x, frame_y, frame_w,
                              frame_h, vars);
        } else if (el->type == OVERLAY_LABEL) {
            draw_label_locked(overlay, el, cr, frame_x, frame_y, frame_w,
                              frame_h, vars);
        } else if (el->type == OVERLAY_LINE) {
            draw_line_locked(overlay, el, cr, frame_x, frame_y, frame_w,
                             frame_h, vars);
        }
    }
    g_ptr_array_free(sorted, TRUE);
    g_mutex_unlock(&overlay->lock);
    if (snapshot) {
        g_hash_table_destroy(snapshot);
    }
}

void overlay_draw(struct overlay_state *overlay, cairo_t *cr,
                  double frame_x, double frame_y, double frame_w,
                  double frame_h)
{
    overlay_draw_with_store(overlay, NULL, cr, frame_x, frame_y, frame_w,
                            frame_h);
}
