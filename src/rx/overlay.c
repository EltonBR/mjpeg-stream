#include "overlay.h"
#include "overlay_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void overlay_element_free(gpointer data)
{
    struct overlay_element *el = data;
    if (!el) {
        return;
    }
    free(el->id);
    free(el->file);
    free(el->text);
    free(el->rotation_expr);
    if (el->pixbuf) {
        g_object_unref(el->pixbuf);
    }
    free(el);
}

void overlay_init(struct overlay_state *overlay)
{
    memset(overlay, 0, sizeof(*overlay));
    overlay->hud_r = 0.0;
    overlay->hud_g = 1.0;
    overlay->hud_b = 0.0;
    overlay->hud_font = strdup("Monospace");
    overlay->dim_r = 0.0;
    overlay->dim_g = 0.0;
    overlay->dim_b = 0.0;
    overlay->dim_alpha = 0.20;
    overlay->elements = g_ptr_array_new_with_free_func(overlay_element_free);
    overlay->widgets = overlay_widgets_new();
    overlay->vars = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_mutex_init(&overlay->lock);
}

int overlay_set_hud_color(struct overlay_state *overlay, const char *color)
{
    double r;
    double g;
    double b;

    if (!color || strcmp(color, "green") == 0 || strcmp(color, "verde") == 0) {
        r = 0.0;
        g = 1.0;
        b = 0.0;
    } else if (strcmp(color, "amber") == 0 || strcmp(color, "ambar") == 0) {
        r = 1.0;
        g = 0.62;
        b = 0.0;
    } else if (overlay_parse_color(color, &r, &g, &b) < 0) {
        fprintf(stderr, "overlay: hud_color invalido: %s\n", color);
        return -1;
    }

    g_mutex_lock(&overlay->lock);
    overlay->hud_r = r;
    overlay->hud_g = g;
    overlay->hud_b = b;
    g_mutex_unlock(&overlay->lock);
    return 0;
}

int overlay_set_hud_font(struct overlay_state *overlay, const char *font)
{
    char *copy;

    if (!font || !font[0]) {
        font = "Monospace";
    }
    copy = strdup(font);
    if (!copy) {
        return -1;
    }

    g_mutex_lock(&overlay->lock);
    free(overlay->hud_font);
    overlay->hud_font = copy;
    g_mutex_unlock(&overlay->lock);
    return 0;
}

int overlay_set_dim(struct overlay_state *overlay, const char *color,
                    double alpha)
{
    double r;
    double g;
    double b;

    if (!color || !color[0]) {
        color = "#000000";
    }
    if (overlay_parse_color(color, &r, &g, &b) < 0) {
        fprintf(stderr, "overlay: dim_color invalido: %s\n", color);
        return -1;
    }
    if (alpha < 0.0) {
        alpha = 0.0;
    } else if (alpha > 1.0) {
        alpha = 1.0;
    }

    g_mutex_lock(&overlay->lock);
    overlay->dim_r = r;
    overlay->dim_g = g;
    overlay->dim_b = b;
    overlay->dim_alpha = alpha;
    g_mutex_unlock(&overlay->lock);
    return 0;
}

void overlay_set_dim_alpha(struct overlay_state *overlay, double alpha)
{
    if (alpha < 0.0) {
        alpha = 0.0;
    } else if (alpha > 1.0) {
        alpha = 1.0;
    }

    g_mutex_lock(&overlay->lock);
    overlay->dim_alpha = alpha;
    g_mutex_unlock(&overlay->lock);
}

double overlay_get_dim_alpha(struct overlay_state *overlay)
{
    double alpha;

    g_mutex_lock(&overlay->lock);
    alpha = overlay->dim_alpha;
    g_mutex_unlock(&overlay->lock);
    return alpha;
}

void overlay_cleanup(struct overlay_state *overlay)
{
    free(overlay->assets_dir);
    overlay->assets_dir = NULL;
    free(overlay->hud_font);
    overlay->hud_font = NULL;
    if (overlay->elements) {
        g_ptr_array_free(overlay->elements, TRUE);
        overlay->elements = NULL;
    }
    if (overlay->widgets) {
        g_ptr_array_free(overlay->widgets, TRUE);
        overlay->widgets = NULL;
    }
    if (overlay->vars) {
        g_hash_table_destroy(overlay->vars);
        overlay->vars = NULL;
    }
    g_mutex_clear(&overlay->lock);
}
