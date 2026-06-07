#include "app.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void rx_app_init(struct rx_app *app, int use_udp, const char *joystick_device,
                 int joystick_enabled, int lock_aspect,
                 const char *zoom_in_key, const char *zoom_out_key,
                 const char *dim_alpha_up_key,
                 const char *dim_alpha_down_key,
                 double dim_alpha_step)
{
    memset(app, 0, sizeof(*app));
    app->fd = -1;
    app->listen_fd = -1;
    app->use_udp = use_udp;
    app->frame = g_byte_array_new();
    app->zoom = 1.0;
    app->zoom_in_key = zoom_in_key;
    app->zoom_out_key = zoom_out_key;
    app->dim_alpha_up_key = dim_alpha_up_key;
    app->dim_alpha_down_key = dim_alpha_down_key;
    app->dim_alpha_step = dim_alpha_step > 0.0 ? dim_alpha_step : 0.05;
    app->lock_aspect = lock_aspect;
    app->joystick_device = joystick_device;
    app->joystick_enabled = joystick_enabled;
    event_sender_init(&app->events);
    overlay_init(&app->overlay);
    telemetry_init(&app->telemetry);
}

void rx_app_cleanup(struct rx_app *app)
{
    /* PONTO CRÍTICO: Cleanup de Joystick
     * ORDEM IMPORTA para evitar deadlock:
     * 1. Sinaliza thread parar (app->stopping = 1)
     * 2. Aguarda término com g_thread_join()
     * Nunca inverter a ordem! */
    app->stopping = 1;
    
    if (app->joystick_thread) {
        fprintf(stderr, "Encerrando thread de joystick...");
        fflush(stderr);
        
        /* Aguarda término da thread (máx ~5s de timeout implícito)
         * Se travar aqui, significa que app->stopping não foi lido
         * pela thread - verificar joystick_thread() */
        GThread *thread = app->joystick_thread;
        app->joystick_thread = NULL;  /* Marca NULL antes para evitar reentrada */
        
        g_thread_join(thread);
        fprintf(stderr, " OK\n");
        fflush(stderr);
    }
    
    telemetry_close(&app->telemetry);
    event_sender_close(&app->events);
    overlay_cleanup(&app->overlay);

    free(app->udp_seen);
    app->udp_seen = NULL;

    if (app->last_pixbuf) {
        g_object_unref(app->last_pixbuf);
        app->last_pixbuf = NULL;
    }
    if (app->frame) {
        g_byte_array_unref(app->frame);
        app->frame = NULL;
    }
    if (app->fd >= 0) {
        close(app->fd);
        app->fd = -1;
    }
    if (app->listen_fd >= 0) {
        close(app->listen_fd);
        app->listen_fd = -1;
    }
}
