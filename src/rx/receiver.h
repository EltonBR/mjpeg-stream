#ifndef MJPEG_RX_RECEIVER_H
#define MJPEG_RX_RECEIVER_H

#include "app.h"
#include "config.h"

int rx_connect(struct rx_app *app, const struct rx_config *cfg);
int rx_start_receiver(struct rx_app *app);
void rx_stop_receiver(struct rx_app *app);

#endif
