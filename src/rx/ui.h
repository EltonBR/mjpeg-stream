#ifndef MJPEG_RX_UI_H
#define MJPEG_RX_UI_H

#include "app.h"

GtkWidget *rx_create_window(struct rx_app *app);
void rx_queue_frame(struct rx_app *app, const unsigned char *data, size_t len);
void rx_queue_redraw(struct rx_app *app);

#endif
