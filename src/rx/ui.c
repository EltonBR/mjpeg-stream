#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct frame_msg {
    struct rx_app *app;
    unsigned char *data;
    size_t len;
};

static void update_zoom_label(struct rx_app *app)
{
    char text[32];
    snprintf(text, sizeof(text), "%.0f%%", app->zoom * 100.0);
    gtk_label_set_text(GTK_LABEL(app->zoom_label), text);
}

static void update_image(struct rx_app *app)
{
    if (app->last_pixbuf && app->drawing_area) {
        int width = (int)(gdk_pixbuf_get_width(app->last_pixbuf) * app->zoom);
        int height = (int)(gdk_pixbuf_get_height(app->last_pixbuf) * app->zoom);
        if (width < 1) {
            width = 1;
        }
        if (height < 1) {
            height = 1;
        }
        gtk_widget_set_size_request(app->drawing_area, width, height);
    }
    update_zoom_label(app);
    rx_queue_redraw(app);
}

static void on_zoom_in(GtkButton *button, gpointer data)
{
    struct rx_app *app = data;
    (void)button;
    if (app->zoom < 4.0) {
        app->zoom *= 1.10;
        if (app->zoom > 4.0) {
            app->zoom = 4.0;
        }
        update_image(app);
    }
}

static void on_zoom_out(GtkButton *button, gpointer data)
{
    struct rx_app *app = data;
    (void)button;
    if (app->zoom > 0.25) {
        app->zoom /= 1.10;
        if (app->zoom < 0.25) {
            app->zoom = 0.25;
        }
        update_image(app);
    }
}

static gboolean show_frame(gpointer data)
{
    struct frame_msg *msg = data;
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf;
    GError *err = NULL;

    loader = gdk_pixbuf_loader_new_with_type("jpeg", &err);
    if (!loader) {
        g_printerr("jpeg loader: %s\n", err ? err->message : "erro desconhecido");
        g_clear_error(&err);
        free(msg->data);
        free(msg);
        return G_SOURCE_REMOVE;
    }

    if (!gdk_pixbuf_loader_write(loader, msg->data, msg->len, &err) ||
        !gdk_pixbuf_loader_close(loader, &err)) {
        g_printerr("jpeg invalido: %s\n", err ? err->message : "erro desconhecido");
        g_clear_error(&err);
        g_object_unref(loader);
        free(msg->data);
        free(msg);
        return G_SOURCE_REMOVE;
    }

    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    if (pixbuf) {
        if (msg->app->last_pixbuf) {
            g_object_unref(msg->app->last_pixbuf);
        }
        msg->app->last_pixbuf = g_object_ref(pixbuf);
        update_image(msg->app);
    }

    g_object_unref(loader);
    free(msg->data);
    free(msg);
    return G_SOURCE_REMOVE;
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    struct rx_app *app = data;
    int widget_w = gtk_widget_get_allocated_width(widget);
    int widget_h = gtk_widget_get_allocated_height(widget);
    int frame_w;
    int frame_h;
    double draw_w;
    double draw_h;
    double draw_x;
    double draw_y;

    cairo_set_source_rgb(cr, 0.04, 0.04, 0.04);
    cairo_paint(cr);

    if (!app->last_pixbuf) {
        return FALSE;
    }

    frame_w = gdk_pixbuf_get_width(app->last_pixbuf);
    frame_h = gdk_pixbuf_get_height(app->last_pixbuf);
    draw_w = frame_w * app->zoom;
    draw_h = frame_h * app->zoom;
    draw_x = (widget_w - draw_w) / 2.0;
    draw_y = (widget_h - draw_h) / 2.0;

    cairo_save(cr);
    cairo_translate(cr, draw_x, draw_y);
    cairo_scale(cr, app->zoom, app->zoom);
    gdk_cairo_set_source_pixbuf(cr, app->last_pixbuf, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);

    overlay_draw(&app->overlay, cr, draw_x, draw_y, draw_w, draw_h);
    return FALSE;
}

void rx_queue_redraw(struct rx_app *app)
{
    if (app->drawing_area) {
        gtk_widget_queue_draw(app->drawing_area);
    }
}

void rx_queue_frame(struct rx_app *app, const unsigned char *data, size_t len)
{
    struct frame_msg *msg = malloc(sizeof(*msg));
    if (!msg) {
        return;
    }
    msg->app = app;
    msg->data = malloc(len);
    if (!msg->data) {
        free(msg);
        return;
    }
    memcpy(msg->data, data, len);
    msg->len = len;
    g_idle_add(show_frame, msg);
}

static void on_destroy(GtkWidget *widget, gpointer data)
{
    struct rx_app *app = data;
    (void)widget;
    if (app->fd >= 0) {
        close(app->fd);
        app->fd = -1;
    }
    if (app->listen_fd >= 0) {
        close(app->listen_fd);
        app->listen_fd = -1;
    }
    gtk_main_quit();
}

GtkWidget *rx_create_window(struct rx_app *app)
{
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *toolbar;
    GtkWidget *zoom_out;
    GtkWidget *zoom_in;
    GtkWidget *scroller;
    GtkWidget *label;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "MJPEG Receiver");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), app);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(window), box);

    label = gtk_label_new(app->use_udp ? "Recebendo MJPEG via UDP" :
                                         "Recebendo MJPEG via TCP");
    toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(toolbar), label, FALSE, FALSE, 0);

    zoom_out = gtk_button_new_with_label("-");
    gtk_widget_set_tooltip_text(zoom_out, "Zoom out");
    gtk_box_pack_end(GTK_BOX(toolbar), zoom_out, FALSE, FALSE, 0);

    zoom_in = gtk_button_new_with_label("+");
    gtk_widget_set_tooltip_text(zoom_in, "Zoom in");
    gtk_box_pack_end(GTK_BOX(toolbar), zoom_in, FALSE, FALSE, 0);

    app->zoom_label = gtk_label_new("100%");
    gtk_box_pack_end(GTK_BOX(toolbar), app->zoom_label, FALSE, FALSE, 0);

    g_signal_connect(zoom_in, "clicked", G_CALLBACK(on_zoom_in), app);
    g_signal_connect(zoom_out, "clicked", G_CALLBACK(on_zoom_out), app);

    gtk_box_pack_start(GTK_BOX(box), toolbar, FALSE, FALSE, 0);

    app->drawing_area = gtk_drawing_area_new();
    app->image = app->drawing_area;
    gtk_widget_set_hexpand(app->drawing_area, TRUE);
    gtk_widget_set_vexpand(app->drawing_area, TRUE);
    gtk_widget_set_size_request(app->drawing_area, 800, 600);
    gtk_widget_add_events(app->drawing_area,
                          GDK_POINTER_MOTION_MASK |
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(app->drawing_area, "draw", G_CALLBACK(on_draw), app);

    scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);

    gtk_container_add(GTK_CONTAINER(scroller), app->drawing_area);
    gtk_box_pack_start(GTK_BOX(box), scroller, TRUE, TRUE, 0);

    return window;
}
