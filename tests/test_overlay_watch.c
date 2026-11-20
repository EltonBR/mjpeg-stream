#include "rx/overlay_watch.h"
#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <unistd.h>

static void pump(void)
{
    gint64 deadline = g_get_monotonic_time() + G_TIME_SPAN_SECOND;
    while (g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(10000);
    }
}
int main(void)
{
    char *dir = g_dir_make_tmp("overlay-watch-XXXXXX", NULL);
    char *path = g_build_filename(dir, "overlay.json", NULL);
    char *temp = g_build_filename(dir, "new.json", NULL);
    struct overlay_state overlay;
    struct overlay_watch watch;
    assert(dir && path && temp);
    assert(g_file_set_contents(path, "{\"widgets\":[]}", -1, NULL));
    overlay_init(&overlay);
    assert(overlay_load_file(&overlay, path) == 0);
    overlay_apply_json_line(&overlay, "{\"type\":\"telemetry\",\"heading\":42}");
    assert(overlay_watch_start(&watch, &overlay, NULL, path) == 0);
    assert(g_file_set_contents(temp, "{\"widgets\":[{\"id\":\"one\",\"type\":\"status\"}]}", -1, NULL));
    assert(rename(temp, path) == 0);
    pump();
    assert(overlay.widgets->len == 1);
    assert(g_strcmp0(g_hash_table_lookup(overlay.vars, "heading"), "42") == 0);
    assert(g_file_set_contents(temp, "{\"widgets\":[{\"id\":\"x\",\"type\":\"unknown\"}]}", -1, NULL));
    assert(rename(temp, path) == 0);
    pump();
    assert(overlay.widgets->len == 1);
    overlay_watch_stop(&watch);
    overlay_cleanup(&overlay);
    unlink(path);
    rmdir(dir);
    g_free(temp); g_free(path); g_free(dir);
    puts("overlay watch reload and rollback passed");
    return 0;
}
