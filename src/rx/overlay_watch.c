#include "overlay_watch.h"
#include <stdio.h>
#include <string.h>

static gboolean reload_layout(gpointer data)
{
    struct overlay_watch *watch = data;
    watch->timeout = 0;
    if (overlay_load_file(watch->overlay, watch->path) == 0) {
        fprintf(stderr, "overlay: layout recarregado: %s\n", watch->path);
        if (watch->widget) gtk_widget_queue_draw(watch->widget);
    }
    return G_SOURCE_REMOVE;
}

static void changed(GFileMonitor *monitor, GFile *file, GFile *other,
                    GFileMonitorEvent event, gpointer data)
{
    struct overlay_watch *watch = data;
    char *path = file ? g_file_get_path(file) : NULL;
    char *other_path = other ? g_file_get_path(other) : NULL;
    (void)monitor;
    (void)event;
    if ((path && strcmp(path, watch->path) == 0) ||
        (other_path && strcmp(other_path, watch->path) == 0)) {
        if (watch->timeout) g_source_remove(watch->timeout);
        watch->timeout = g_timeout_add(150, reload_layout, watch);
    }
    g_free(path);
    g_free(other_path);
}

int overlay_watch_start(struct overlay_watch *watch, struct overlay_state *overlay,
                        GtkWidget *widget, const char *path)
{
    GFile *directory;
    char *dirname;
    memset(watch, 0, sizeof(*watch));
    watch->path = g_canonicalize_filename(path, NULL);
    watch->overlay = overlay;
    watch->widget = widget;
    dirname = g_path_get_dirname(watch->path);
    directory = g_file_new_for_path(dirname);
    watch->monitor = g_file_monitor_directory(directory, G_FILE_MONITOR_WATCH_MOVES,
                                               NULL, NULL);
    g_object_unref(directory);
    g_free(dirname);
    if (!watch->monitor) {
        g_free(watch->path);
        watch->path = NULL;
        return -1;
    }
    g_signal_connect(watch->monitor, "changed", G_CALLBACK(changed), watch);
    return 0;
}

void overlay_watch_stop(struct overlay_watch *watch)
{
    if (watch->timeout) g_source_remove(watch->timeout);
    if (watch->monitor) {
        g_file_monitor_cancel(watch->monitor);
        g_object_unref(watch->monitor);
    }
    g_free(watch->path);
    memset(watch, 0, sizeof(*watch));
}
