#include "app.h"
#include "config.h"
#include "event_sender.h"
#include "input.h"
#include "overlay.h"
#include "receiver.h"
#include "telemetry.h"
#include "ui.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    struct rx_config cfg;
    struct rx_app app;
    GtkWidget *window;

    if (rx_parse_args(argc, argv, &cfg) < 0) {
        rx_usage(argv[0]);
        return 2;
    }

    rx_app_init(&app, cfg.use_udp, cfg.joystick_device, cfg.joystick_enabled,
                cfg.lock_aspect);
    gtk_init(&argc, &argv);

    if (overlay_set_hud_color(&app.overlay, cfg.hud_color) < 0) {
        rx_app_cleanup(&app);
        return 1;
    }
    if (overlay_set_hud_font(&app.overlay, cfg.hud_font) < 0) {
        rx_app_cleanup(&app);
        return 1;
    }
    if (overlay_set_dim(&app.overlay, cfg.dim_color, cfg.dim_alpha) < 0) {
        rx_app_cleanup(&app);
        return 1;
    }

    if (cfg.overlay_path) {
        if (access(cfg.overlay_path, R_OK) == 0) {
            if (overlay_load_file(&app.overlay, cfg.overlay_path) < 0) {
                rx_app_cleanup(&app);
                return 1;
            }
        } else if (cfg.overlay_required) {
            fprintf(stderr, "overlay: arquivo nao encontrado: %s\n",
                    cfg.overlay_path);
            rx_app_cleanup(&app);
            return 1;
        }
    }

    if (rx_connect(&app, &cfg) < 0) {
        rx_app_cleanup(&app);
        return 1;
    }

    window = rx_create_window(&app);
    if (cfg.events_enabled) {
        event_sender_start(&app.events, cfg.event_host, cfg.event_port);
    }
    if (cfg.telemetry_enabled) {
        telemetry_start(&app.telemetry, cfg.telemetry_host, cfg.telemetry_port,
                        &app.overlay, app.drawing_area);
    }
    if (app.events.enabled) {
        rx_input_attach(window, &app);
        rx_input_start_joystick(&app);
    }
    gtk_widget_show_all(window);
    rx_start_receiver(&app);

    gtk_main();

    rx_app_cleanup(&app);
    return 0;
}
