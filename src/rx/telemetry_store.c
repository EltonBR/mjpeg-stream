#include "telemetry_store.h"

#include <json-c/json.h>

#include <string.h>

struct telemetry_store {
    GMutex mutex;
    GHashTable *values;
};

TelemetryStore *telemetry_store_new(void)
{
    TelemetryStore *store = g_new0(TelemetryStore, 1);

    g_mutex_init(&store->mutex);
    store->values = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          g_free, g_free);
    return store;
}

void telemetry_store_free(TelemetryStore *store)
{
    if (!store) {
        return;
    }

    g_hash_table_destroy(store->values);
    g_mutex_clear(&store->mutex);
    g_free(store);
}

gboolean telemetry_store_update_json(TelemetryStore *store, const char *json)
{
    struct json_object *root;
    struct json_object *type_value;
    const char *type;

    if (!store || !json) {
        return FALSE;
    }

    root = json_tokener_parse(json);
    if (!root || json_object_get_type(root) != json_type_object ||
        !json_object_object_get_ex(root, "type", &type_value) ||
        json_object_get_type(type_value) != json_type_string) {
        if (root) {
            json_object_put(root);
        }
        return FALSE;
    }

    type = json_object_get_string(type_value);
    if (strcmp(type, "telemetry") != 0) {
        json_object_put(root);
        return FALSE;
    }

    g_mutex_lock(&store->mutex);
    json_object_object_foreach(root, key, value) {
        const char *stored = NULL;
        enum json_type value_type;

        if (strcmp(key, "type") == 0) {
            continue;
        }

        value_type = json_object_get_type(value);
        if (value_type == json_type_int || value_type == json_type_double ||
            value_type == json_type_string) {
            stored = json_object_get_string(value);
        } else if (value_type == json_type_boolean) {
            stored = json_object_get_boolean(value) ? "true" : "false";
        }

        if (stored) {
            g_hash_table_replace(store->values, g_strdup(key),
                                 g_strdup(stored));
        }
    }
    g_mutex_unlock(&store->mutex);

    json_object_put(root);
    return TRUE;
}

GHashTable *telemetry_store_snapshot(TelemetryStore *store)
{
    GHashTable *snapshot;
    GHashTableIter iter;
    gpointer key;
    gpointer value;

    if (!store) {
        return NULL;
    }

    snapshot = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_mutex_lock(&store->mutex);
    g_hash_table_iter_init(&iter, store->values);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        g_hash_table_insert(snapshot, g_strdup(key), g_strdup(value));
    }
    g_mutex_unlock(&store->mutex);

    return snapshot;
}
