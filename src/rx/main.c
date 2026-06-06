#include "app.h"
#include "config.h"
#include "event_sender.h"
#include "input.h"
#include "receiver.h"
#include "ui.h"

#include <gtk/gtk.h>

int main(int argc, char **argv)
{
    struct rx_config cfg;
    struct rx_app app;
    GtkWidget *window;

    if (rx_parse_args(argc, argv, &cfg) < 0) {
        rx_usage(argv[0]);
        return 2;
    }

    rx_app_init(&app, cfg.use_udp, cfg.joystick_device, cfg.joystick_enabled);
    gtk_init(&argc, &argv);

    if (rx_connect(&app, &cfg) < 0) {
        rx_app_cleanup(&app);
        return 1;
    }

    if (cfg.events_enabled) {
        event_sender_start(&app.events, cfg.event_host, cfg.event_port);
    }

    window = rx_create_window(&app);
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
