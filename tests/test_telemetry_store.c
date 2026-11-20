#include "telemetry_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_scalar_updates(void)
{
    TelemetryStore *store = telemetry_store_new();
    GHashTable *snapshot;

    assert(telemetry_store_update_json(
        store,
        "{\"type\":\"telemetry\",\"flag\":true,\"count\":12,"
        "\"ratio\":2.5,\"label\":\"north\",\"ignored\":null,"
        "\"nested\":{\"x\":1},\"items\":[1,2]}"));

    snapshot = telemetry_store_snapshot(store);
    assert(snapshot != NULL);
    assert(strcmp(g_hash_table_lookup(snapshot, "flag"), "true") == 0);
    assert(strcmp(g_hash_table_lookup(snapshot, "count"), "12") == 0);
    assert(strcmp(g_hash_table_lookup(snapshot, "ratio"), "2.5") == 0);
    assert(strcmp(g_hash_table_lookup(snapshot, "label"), "north") == 0);
    assert(!g_hash_table_contains(snapshot, "type"));
    assert(!g_hash_table_contains(snapshot, "ignored"));
    assert(!g_hash_table_contains(snapshot, "nested"));
    assert(!g_hash_table_contains(snapshot, "items"));

    g_hash_table_destroy(snapshot);
    telemetry_store_free(store);
}

static void test_updates_and_independent_snapshot(void)
{
    TelemetryStore *store = telemetry_store_new();
    GHashTable *snapshot;

    assert(telemetry_store_update_json(
        store, "{\"type\":\"telemetry\",\"value\":\"first\"}"));
    snapshot = telemetry_store_snapshot(store);
    assert(telemetry_store_update_json(
        store, "{\"type\":\"telemetry\",\"value\":\"second\"}"));

    assert(strcmp(g_hash_table_lookup(snapshot, "value"), "first") == 0);
    {
        GHashTable *fresh = telemetry_store_snapshot(store);
        assert(strcmp(g_hash_table_lookup(fresh, "value"), "second") == 0);
        g_hash_table_destroy(fresh);
    }

    g_hash_table_destroy(snapshot);
    /* A caller can modify its copy without changing the store. */
    snapshot = telemetry_store_snapshot(store);
    g_hash_table_replace(snapshot, g_strdup("value"), g_strdup("local"));
    {
        GHashTable *fresh = telemetry_store_snapshot(store);
        assert(strcmp(g_hash_table_lookup(fresh, "value"), "second") == 0);
        g_hash_table_destroy(fresh);
    }
    g_hash_table_destroy(snapshot);
    telemetry_store_free(store);
}

static void test_rejects_invalid_messages(void)
{
    TelemetryStore *store = telemetry_store_new();
    GHashTable *snapshot;

    assert(!telemetry_store_update_json(store, "not json"));
    assert(!telemetry_store_update_json(store, "[]"));
    assert(!telemetry_store_update_json(store, "{\"value\":1}"));
    assert(!telemetry_store_update_json(store,
                                        "{\"type\":\"other\",\"x\":1}"));
    assert(!telemetry_store_update_json(NULL,
                                        "{\"type\":\"telemetry\"}"));
    assert(!telemetry_store_update_json(store, NULL));
    snapshot = telemetry_store_snapshot(store);
    assert(g_hash_table_size(snapshot) == 0);
    g_hash_table_destroy(snapshot);
    telemetry_store_free(store);
    assert(telemetry_store_snapshot(NULL) == NULL);
}

int main(void)
{
    test_scalar_updates();
    test_updates_and_independent_snapshot();
    test_rejects_invalid_messages();
    puts("telemetry store passed");
    return 0;
}
