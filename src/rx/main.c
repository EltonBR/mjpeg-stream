#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct config {
    const char *host;
    const char *listen_host;
    const char *port;
    int use_udp;
};

struct app {
    GtkWidget *image;
    GtkWidget *zoom_label;
    GdkPixbuf *last_pixbuf;
    double zoom;
    int fd;
    int listen_fd;
    int use_udp;
    GByteArray *frame;
    uint32_t udp_frame_id;
    uint16_t udp_chunk_count;
    uint16_t udp_received;
    unsigned char *udp_seen;
};

struct frame_msg {
    struct app *app;
    unsigned char *data;
    size_t len;
};

static void update_zoom_label(struct app *app)
{
    char text[32];
    snprintf(text, sizeof(text), "%.0f%%", app->zoom * 100.0);
    gtk_label_set_text(GTK_LABEL(app->zoom_label), text);
}

static void update_image(struct app *app)
{
    GdkPixbuf *scaled;
    int width;
    int height;

    if (!app->last_pixbuf) {
        return;
    }

    width = (int)(gdk_pixbuf_get_width(app->last_pixbuf) * app->zoom);
    height = (int)(gdk_pixbuf_get_height(app->last_pixbuf) * app->zoom);
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }

    scaled = gdk_pixbuf_scale_simple(app->last_pixbuf, width, height,
                                     GDK_INTERP_BILINEAR);
    if (!scaled) {
        return;
    }

    gtk_image_set_from_pixbuf(GTK_IMAGE(app->image), scaled);
    g_object_unref(scaled);
    update_zoom_label(app);
}

static void on_zoom_in(GtkButton *button, gpointer data)
{
    struct app *app = data;
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
    struct app *app = data;
    (void)button;
    if (app->zoom > 0.25) {
        app->zoom /= 1.10;
        if (app->zoom < 0.25) {
            app->zoom = 0.25;
        }
        update_image(app);
    }
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Uso TCP: %s --host 127.0.0.1 --port 5000 --tcp\n"
            "Uso UDP: %s --listen 0.0.0.0 --port 5000 --udp\n",
            argv0, argv0);
}

static int parse_args(int argc, char **argv, struct config *cfg)
{
    int i;
    cfg->host = "127.0.0.1";
    cfg->listen_host = "0.0.0.0";
    cfg->port = "5000";
    cfg->use_udp = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            cfg->host = argv[++i];
        } else if (strcmp(argv[i], "--listen") == 0 && i + 1 < argc) {
            cfg->listen_host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            cfg->port = argv[++i];
        } else if (strcmp(argv[i], "--udp") == 0) {
            cfg->use_udp = 1;
        } else if (strcmp(argv[i], "--tcp") == 0) {
            cfg->use_udp = 0;
        } else {
            return -1;
        }
    }
    return 0;
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

static void queue_frame(struct app *app, const unsigned char *data, size_t len)
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

static gpointer tcp_thread(gpointer data)
{
    struct app *app = data;
    while (1) {
        uint32_t nlen;
        uint32_t len;
        unsigned char *buf;
        int rc = read_all(app->fd, &nlen, sizeof(nlen));
        if (rc <= 0) {
            break;
        }
        len = ntohl(nlen);
        if (len == 0 || len > MJPEG_MAX_FRAME) {
            break;
        }
        buf = malloc(len);
        if (!buf) {
            break;
        }
        rc = read_all(app->fd, buf, len);
        if (rc <= 0) {
            free(buf);
            break;
        }
        queue_frame(app, buf, len);
        free(buf);
    }
    g_idle_add((GSourceFunc)gtk_main_quit, NULL);
    return NULL;
}

static gpointer udp_thread(gpointer data)
{
    struct app *app = data;
    unsigned char packet[sizeof(struct udp_frame_header) + MJPEG_UDP_PAYLOAD + 64u];

    while (1) {
        ssize_t n = recv_intr(app->fd, packet, sizeof(packet), 0);
        struct udp_frame_header hdr;
        uint32_t magic;
        uint32_t frame_id;
        uint16_t chunk_id;
        uint16_t chunk_count;
        uint32_t frame_size;
        size_t payload;
        size_t off;

        if (n <= (ssize_t)sizeof(hdr)) {
            if (n < 0) {
                perror("recv udp");
            }
            continue;
        }

        memcpy(&hdr, packet, sizeof(hdr));
        magic = ntohl(hdr.magic);
        frame_id = ntohl(hdr.frame_id);
        chunk_id = ntohs(hdr.chunk_id);
        chunk_count = ntohs(hdr.chunk_count);
        frame_size = ntohl(hdr.frame_size);
        payload = (size_t)n - sizeof(hdr);

        if (magic != MJPEG_UDP_MAGIC || frame_size == 0 ||
            frame_size > MJPEG_MAX_FRAME || chunk_count == 0 ||
            chunk_id >= chunk_count) {
            continue;
        }

        if (frame_id != app->udp_frame_id || app->frame->len != frame_size) {
            app->udp_frame_id = frame_id;
            app->udp_chunk_count = chunk_count;
            app->udp_received = 0;
            g_byte_array_set_size(app->frame, frame_size);
            free(app->udp_seen);
            app->udp_seen = calloc(chunk_count, 1);
            if (!app->udp_seen) {
                g_byte_array_set_size(app->frame, 0);
                continue;
            }
        }

        off = (size_t)chunk_id * MJPEG_UDP_PAYLOAD;
        if (off + payload > app->frame->len) {
            continue;
        }

        memcpy(app->frame->data + off, packet + sizeof(hdr), payload);
        if (!app->udp_seen[chunk_id]) {
            app->udp_seen[chunk_id] = 1;
            app->udp_received++;
        }

        if (app->udp_received == app->udp_chunk_count) {
            queue_frame(app, app->frame->data, app->frame->len);
            g_byte_array_set_size(app->frame, 0);
        }
    }
    return NULL;
}

static void on_destroy(GtkWidget *widget, gpointer data)
{
    struct app *app = data;
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

int main(int argc, char **argv)
{
    struct config cfg;
    struct app app;
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *toolbar;
    GtkWidget *zoom_out;
    GtkWidget *zoom_in;
    GtkWidget *scroller;
    GtkWidget *viewport;
    GtkWidget *label;

    if (parse_args(argc, argv, &cfg) < 0) {
        usage(argv[0]);
        return 2;
    }

    memset(&app, 0, sizeof(app));
    app.fd = -1;
    app.listen_fd = -1;
    app.use_udp = cfg.use_udp;
    app.frame = g_byte_array_new();
    app.zoom = 1.0;

    gtk_init(&argc, &argv);

    if (cfg.use_udp) {
        app.fd = udp_bind_socket(cfg.listen_host, cfg.port);
        if (app.fd < 0) {
            fprintf(stderr, "falha ao escutar UDP em %s:%s\n", cfg.listen_host, cfg.port);
            return 1;
        }
    } else {
        fprintf(stderr, "conectando ao transmissor TCP em %s:%s...\n",
                cfg.host, cfg.port);
        app.fd = tcp_connect(cfg.host, cfg.port);
        if (app.fd < 0) {
            fprintf(stderr, "falha ao conectar em %s:%s\n", cfg.host, cfg.port);
            return 1;
        }
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "MJPEG Receiver");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), &app);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(window), box);

    label = gtk_label_new(cfg.use_udp ? "Recebendo MJPEG via UDP" : "Recebendo MJPEG via TCP");
    toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(toolbar), label, FALSE, FALSE, 0);

    zoom_out = gtk_button_new_with_label("-");
    gtk_widget_set_tooltip_text(zoom_out, "Zoom out");
    gtk_box_pack_end(GTK_BOX(toolbar), zoom_out, FALSE, FALSE, 0);

    zoom_in = gtk_button_new_with_label("+");
    gtk_widget_set_tooltip_text(zoom_in, "Zoom in");
    gtk_box_pack_end(GTK_BOX(toolbar), zoom_in, FALSE, FALSE, 0);

    app.zoom_label = gtk_label_new("100%");
    gtk_box_pack_end(GTK_BOX(toolbar), app.zoom_label, FALSE, FALSE, 0);

    g_signal_connect(zoom_in, "clicked", G_CALLBACK(on_zoom_in), &app);
    g_signal_connect(zoom_out, "clicked", G_CALLBACK(on_zoom_out), &app);

    gtk_box_pack_start(GTK_BOX(box), toolbar, FALSE, FALSE, 0);

    app.image = gtk_image_new();
    gtk_widget_set_hexpand(app.image, TRUE);
    gtk_widget_set_vexpand(app.image, TRUE);
    gtk_widget_set_halign(app.image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(app.image, GTK_ALIGN_CENTER);

    scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);

    viewport = gtk_viewport_new(NULL, NULL);
    gtk_widget_set_hexpand(viewport, TRUE);
    gtk_widget_set_vexpand(viewport, TRUE);
    gtk_container_add(GTK_CONTAINER(viewport), app.image);
    gtk_container_add(GTK_CONTAINER(scroller), viewport);
    gtk_box_pack_start(GTK_BOX(box), scroller, TRUE, TRUE, 0);

    gtk_widget_show_all(window);

    if (cfg.use_udp) {
        g_thread_new("udp-recv", udp_thread, &app);
    } else {
        g_thread_new("tcp-recv", tcp_thread, &app);
    }

    gtk_main();

    free(app.udp_seen);
    if (app.last_pixbuf) {
        g_object_unref(app.last_pixbuf);
    }
    if (app.frame) {
        g_byte_array_unref(app.frame);
    }
    return 0;
}
