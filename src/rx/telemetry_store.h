#ifndef MJPEG_RX_TELEMETRY_STORE_H
#define MJPEG_RX_TELEMETRY_STORE_H

#include <glib.h>

typedef struct telemetry_store TelemetryStore;

TelemetryStore *telemetry_store_new(void);
void telemetry_store_free(TelemetryStore *store);

/* Applies one JSON telemetry message. Returns FALSE for invalid/non-telemetry JSON. */
gboolean telemetry_store_update_json(TelemetryStore *store, const char *json);

/* Returns an independently owned string-to-string copy; destroy with
 * g_hash_table_destroy(). */
GHashTable *telemetry_store_snapshot(TelemetryStore *store);

#endif
