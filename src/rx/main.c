#include "app.h"
#include "config.h"
#include "event_sender.h"
#include "input.h"
#include "overlay.h"
#include "overlay_config.h"
#include "overlay_watch.h"
#include "receiver.h"
#include "telemetry.h"
#include "ui.h"

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <locale.h>

static void telemetry_redraw(gpointer user_data)
{
    GtkWidget *drawing_area = user_data;

    if (drawing_area && gtk_widget_get_realized(drawing_area) &&
        !gtk_widget_in_destruction(drawing_area)) {
        gtk_widget_queue_draw(drawing_area);
    }
}

int main(int argc, char **argv)
{
    struct rx_config cfg;
    struct overlay_config overlay_cfg;
    struct rx_app app;
    GtkWidget *window;
    GtkWidget *telemetry_widget = NULL;
    struct overlay_watch watch = {0};

    (void)setlocale(LC_ALL, "");
    (void)setlocale(LC_NUMERIC, "C");
    gtk_disable_setlocale();

    if (rx_parse_args(argc, argv, &cfg) < 0) {
        rx_config_destroy(&cfg);
        rx_usage(argv[0]);
        return 2;
    }
    if (overlay_config_parse_args(argc, argv, &overlay_cfg) < 0) {
        rx_config_destroy(&cfg);
        rx_usage(argv[0]);
        return 2;
    }

    rx_app_init(&app, cfg.use_udp, cfg.joystick_device, cfg.joystick_enabled,
                cfg.lock_aspect, cfg.zoom_in_key, cfg.zoom_out_key,
                cfg.dim_alpha_up_key, cfg.dim_alpha_down_key,
                cfg.dim_alpha_step);
    app.overlay_enabled = overlay_cfg.enabled;
    gtk_init(&argc, &argv);

    if (overlay_set_hud_color(&app.overlay, overlay_cfg.hud_color) < 0) {
        rx_app_cleanup(&app);
        overlay_config_destroy(&overlay_cfg);
        rx_config_destroy(&cfg);
        return 1;
    }
    if (overlay_set_hud_font(&app.overlay, overlay_cfg.hud_font) < 0) {
        rx_app_cleanup(&app);
        overlay_config_destroy(&overlay_cfg);
        rx_config_destroy(&cfg);
        return 1;
    }
    if (overlay_set_dim(&app.overlay, overlay_cfg.dim_color, overlay_cfg.dim_alpha) < 0) {
        rx_app_cleanup(&app);
        overlay_config_destroy(&overlay_cfg);
        rx_config_destroy(&cfg);
        return 1;
    }

    overlay_set_assets_paths(&app.overlay, overlay_cfg.base_dir,
                             overlay_cfg.assets_dir_explicit ? overlay_cfg.assets_dir : NULL);

    if (overlay_cfg.enabled) {
        if (access(overlay_cfg.layout_path, R_OK) == 0) {
            if (overlay_load_file(&app.overlay, overlay_cfg.layout_path) < 0) {
                rx_app_cleanup(&app);
                overlay_config_destroy(&overlay_cfg);
                rx_config_destroy(&cfg);
                return 1;
            }
        } else if (overlay_cfg.layout_required) {
            fprintf(stderr, "overlay: arquivo nao encontrado: %s\n",
                    overlay_cfg.layout_path);
            rx_app_cleanup(&app);
            overlay_config_destroy(&overlay_cfg);
            rx_config_destroy(&cfg);
            return 1;
        }
    }

    if (rx_connect(&app, &cfg) < 0) {
        rx_app_cleanup(&app);
        overlay_config_destroy(&overlay_cfg);
        rx_config_destroy(&cfg);
        return 1;
    }

    window = rx_create_window(&app);
    if (overlay_cfg.enabled &&
        overlay_watch_start(&watch, &app.overlay, app.drawing_area, overlay_cfg.layout_path) < 0)
        fprintf(stderr, "overlay: monitor de layout indisponivel\n");
    if (cfg.events_enabled) {
        event_sender_start(&app.events, cfg.event_host, cfg.event_port);
    }
    if (overlay_cfg.telemetry_enabled) {
        telemetry_widget = g_object_ref(app.drawing_area);
        telemetry_start_with_store(&app.telemetry, overlay_cfg.telemetry_host,
                                   overlay_cfg.telemetry_port, &app.overlay,
                                   app.telemetry_store,
                                   g_main_context_default(), telemetry_redraw,
                                   telemetry_widget,
                                   overlay_cfg.reconnect_ms);
    }
    rx_input_attach(window, &app);
    if (app.events.enabled) {
        rx_input_start_joystick(&app);
    }
    gtk_widget_show_all(window);
    if (rx_start_receiver(&app) < 0) {
        overlay_watch_stop(&watch);
        rx_app_cleanup(&app);
        if (telemetry_widget) g_object_unref(telemetry_widget);
        overlay_config_destroy(&overlay_cfg);
        rx_config_destroy(&cfg);
        return 1;
    }

    gtk_main();

    {
        int result = g_atomic_int_get(&app.receiver_initial_failed) ? 1 : 0;
        overlay_watch_stop(&watch);
        rx_app_cleanup(&app);
        if (telemetry_widget) g_object_unref(telemetry_widget);
        overlay_config_destroy(&overlay_cfg);
        rx_config_destroy(&cfg);
        return result;
    }
}
