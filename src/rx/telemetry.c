#include "telemetry.h"

#include "net.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static gboolean queue_redraw_idle(gpointer data)
{
    GtkWidget *drawing_area = data;
    if (drawing_area) {
        gtk_widget_queue_draw(drawing_area);
    }
    return G_SOURCE_REMOVE;
}

static void schedule_redraw(struct telemetry_client *client)
{
    g_idle_add(queue_redraw_idle, client->drawing_area);
}

static int read_line(int fd, char *buf, size_t len)
{
    size_t used = 0;

    while (used + 1 < len) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return 0;
        }
        if (c == '\n') {
            break;
        }
        if (c != '\r') {
            buf[used++] = c;
        }
    }
    buf[used] = '\0';
    return used > 0 ? 1 : 0;
}

static gpointer telemetry_thread(gpointer data)
{
    struct telemetry_client *client = data;
    char line[8192];

    while (!client->stopping) {
        int fd;

        fd = tcp_connect(client->host, client->port);
        if (fd < 0) {
            g_usleep(1000000);
            continue;
        }

        client->fd = fd;
        fprintf(stderr, "telemetria conectada em %s:%s\n",
                client->host, client->port);

        while (!client->stopping) {
            int rc = read_line(fd, line, sizeof(line));
            if (rc <= 0) {
                break;
            }
            overlay_apply_json_line(client->overlay, line);
            schedule_redraw(client);
        }

        close(fd);
        client->fd = -1;
        if (!client->stopping) {
            fprintf(stderr, "telemetria desconectada, tentando reconectar\n");
            g_usleep(1000000);
        }
    }
    return NULL;
}

void telemetry_init(struct telemetry_client *client)
{
    memset(client, 0, sizeof(*client));
    client->fd = -1;
}

void telemetry_start(struct telemetry_client *client, const char *host,
                     const char *port, struct overlay_state *overlay,
                     GtkWidget *drawing_area)
{
    client->host = host;
    client->port = port;
    client->overlay = overlay;
    client->drawing_area = drawing_area;
    client->enabled = 1;
    client->thread = g_thread_new("telemetry", telemetry_thread, client);
}

void telemetry_close(struct telemetry_client *client)
{
    client->stopping = 1;
    if (client->fd >= 0) {
        close(client->fd);
        client->fd = -1;
    }
    if (client->thread) {
        g_thread_join(client->thread);
        client->thread = NULL;
    }
    client->enabled = 0;
}
