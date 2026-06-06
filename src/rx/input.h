#ifndef MJPEG_RX_INPUT_H
#define MJPEG_RX_INPUT_H

#include "app.h"

void rx_input_attach(GtkWidget *window, struct rx_app *app);
void rx_input_start_joystick(struct rx_app *app);

#endif
