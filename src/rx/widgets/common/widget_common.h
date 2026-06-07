#ifndef MJPEG_RX_WIDGET_COMMON_H
#define MJPEG_RX_WIDGET_COMMON_H

#include <cairo.h>
#include <gtk/gtk.h>
#include <json-c/json.h>

enum widget_type {
    WIDGET_COMPASS,
    WIDGET_READOUT,
    WIDGET_STATUS,
    WIDGET_IMAGE
};

enum widget_anchor {
    WIDGET_ANCHOR_TOP_LEFT,
    WIDGET_ANCHOR_TOP_CENTER,
    WIDGET_ANCHOR_TOP_RIGHT,
    WIDGET_ANCHOR_BOTTOM_LEFT,
    WIDGET_ANCHOR_BOTTOM_RIGHT,
    WIDGET_ANCHOR_CENTER
};

struct overlay_render_context {
    double frame_x;
    double frame_y;
    double frame_w;
    double frame_h;
    double hud_r;
    double hud_g;
    double hud_b;
    const char *assets_dir;
    GHashTable *vars;
};

struct readout_item {
    char *label;
    char *value;
};

struct overlay_widget {
    char *id;
    enum widget_type type;
    enum widget_anchor anchor;
    int visible;
    int z_index;
    double x;
    double y;
    double w;
    double h;
    double alpha;
    double size;
    double width;
    char *file;
    char *text;
    char *azimuth_expr;
    char *rotation_expr;
    char *compass_labels[8];
    GPtrArray *items;
    GdkPixbuf *pixbuf;
    int pixbuf_failed;
};

void overlay_widget_free(gpointer data);
void readout_item_free(gpointer data);

int widget_json_get_obj(struct json_object *obj, const char *key,
                 struct json_object **out);
const char *widget_json_get_string_default(struct json_object *obj, const char *key,
                                    const char *fallback);
char *widget_dup_json_string(struct json_object *obj, const char *key,
                      const char *fallback);
double widget_json_get_double_default(struct json_object *obj, const char *key,
                               double fallback);
int widget_json_get_int_default(struct json_object *obj, const char *key,
                         int fallback);
int widget_json_get_bool_default(struct json_object *obj, const char *key,
                          int fallback);

struct overlay_widget *widget_base(struct json_object *obj,
                                   enum widget_type type);
int widget_valid_image_path(const char *file);
void widget_anchor_origin(enum widget_anchor anchor, double x, double y,
                   double w, double h, double *out_x, double *out_y);
double widget_coord_value(double value, double extent);
char *widget_expand_template(const struct overlay_render_context *ctx,
                      const char *text);
double widget_template_number(const struct overlay_render_context *ctx,
                       const char *expr, double fallback);
int widget_template_number_found(const struct overlay_render_context *ctx,
                                 const char *expr, double *out);
void widget_draw_text(cairo_t *cr, const struct overlay_render_context *ctx,
               const char *text, double x, double y, double size,
               double alpha, enum widget_anchor anchor);
void load_widget_pixbuf(const struct overlay_render_context *ctx,
                        struct overlay_widget *widget);

#endif
