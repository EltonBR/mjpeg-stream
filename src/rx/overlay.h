#ifndef MJPEG_RX_OVERLAY_H
#define MJPEG_RX_OVERLAY_H

#include <cairo.h>
#include <gtk/gtk.h>

#include "overlay_widgets.h"

enum overlay_type {
    OVERLAY_IMAGE,
    OVERLAY_LABEL,
    OVERLAY_LINE
};

enum overlay_anchor {
    OVERLAY_ANCHOR_TOP_LEFT,
    OVERLAY_ANCHOR_TOP_RIGHT,
    OVERLAY_ANCHOR_BOTTOM_LEFT,
    OVERLAY_ANCHOR_BOTTOM_RIGHT,
    OVERLAY_ANCHOR_CENTER
};

struct overlay_element {
    char *id;
    enum overlay_type type;
    char *file;
    char *text;
    char *rotation_expr;
    double x;
    double y;
    double w;
    double h;
    double x1;
    double y1;
    double x2;
    double y2;
    double width;
    double rotation;
    double alpha;
    double color_r;
    double color_g;
    double color_b;
    int visible;
    int z_index;
    enum overlay_anchor anchor;
    GdkPixbuf *pixbuf;
    int pixbuf_failed;
};

struct overlay_state {
    int enabled;
    char *assets_dir;
    double hud_r;
    double hud_g;
    double hud_b;
    char *hud_font;
    double dim_r;
    double dim_g;
    double dim_b;
    double dim_alpha;
    GPtrArray *elements;
    GPtrArray *widgets;
    GHashTable *vars;
    GMutex lock;
};

void overlay_init(struct overlay_state *overlay);
int overlay_set_hud_color(struct overlay_state *overlay, const char *color);
int overlay_set_hud_font(struct overlay_state *overlay, const char *font);
int overlay_set_dim(struct overlay_state *overlay, const char *color,
                    double alpha);
int overlay_load_file(struct overlay_state *overlay, const char *path);
void overlay_cleanup(struct overlay_state *overlay);
void overlay_draw(struct overlay_state *overlay, cairo_t *cr,
                  double frame_x, double frame_y, double frame_w,
                  double frame_h);
void overlay_apply_json_line(struct overlay_state *overlay, const char *line);

#endif
