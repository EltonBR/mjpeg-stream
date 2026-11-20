#include "overlay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned checks;
static unsigned failures;

#define CHECK(description, expression) \
    do { \
        checks++; \
        if (!(expression)) { \
            fprintf(stderr, "FAIL: %s (linha %d)\n", description, __LINE__); \
            failures++; \
        } \
    } while (0)

static int write_temp_file(char *path, size_t path_size, const char *contents)
{
    int fd;
    FILE *file;

    static const char template[] = "/tmp/mjpeg-overlay-test-XXXXXX";
    if (path_size < sizeof(template)) {
        return -1;
    }
    memcpy(path, template, sizeof(template));
    fd = mkstemp(path);
    if (fd < 0) {
        return -1;
    }
    file = fdopen(fd, "w");
    if (!file) {
        close(fd);
        unlink(path);
        return -1;
    }
    if (fputs(contents, file) == EOF) {
        fclose(file);
        unlink(path);
        return -1;
    }
    if (fclose(file) != 0) {
        unlink(path);
        return -1;
    }
    return 0;
}

static struct overlay_element *find_element(struct overlay_state *overlay,
                                            const char *id)
{
    unsigned i;

    for (i = 0; i < overlay->elements->len; i++) {
        struct overlay_element *el = g_ptr_array_index(overlay->elements, i);
        if (el->id && strcmp(el->id, id) == 0) {
            return el;
        }
    }
    return NULL;
}

static unsigned count_green_pixels(cairo_surface_t *surface)
{
    unsigned char *data;
    int stride;
    int width;
    int height;
    int x;
    int y;
    unsigned count = 0;

    cairo_surface_flush(surface);
    data = cairo_image_surface_get_data(surface);
    stride = cairo_image_surface_get_stride(surface);
    width = cairo_image_surface_get_width(surface);
    height = cairo_image_surface_get_height(surface);
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            const unsigned char *pixel = data + y * stride + x * 4;
            /* Cairo ARGB32 is native-endian; inspect channels by byte order. */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            unsigned char blue = pixel[0];
            unsigned char green = pixel[1];
            unsigned char red = pixel[2];
            unsigned char alpha = pixel[3];
#else
            unsigned char alpha = pixel[0];
            unsigned char red = pixel[1];
            unsigned char green = pixel[2];
            unsigned char blue = pixel[3];
#endif
            (void)blue;
            if (alpha > 0 && green > red + 40) {
                count++;
            }
        }
    }
    return count;
}

static void clear_surface(cairo_t *cr)
{
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);
}

static void test_load_json_updates_and_draw(void)
{
    static const char config[] =
        "{\"assets_dir\":\".\",\"elements\":["
        "{\"id\":\"baseline\",\"type\":\"line\","
        "\"x1\":8,\"y1\":8,\"x2\":24,\"y2\":8,\"width\":3},"
        "{\"id\":\"caption\",\"type\":\"label\","
        "\"text\":\"value={value}\",\"x\":2,\"y\":30,\"size\":12}] }";
    static const char malformed[] = "{\"assets_dir\":\"../escape\",\"elements\":[]}";
    char path[sizeof("/tmp/mjpeg-overlay-test-XXXXXX")];
    char bad_path[sizeof("/tmp/mjpeg-overlay-test-XXXXXX")];
    struct overlay_state overlay;
    struct overlay_element *baseline;
    struct overlay_element *caption;
    cairo_surface_t *surface;
    cairo_t *cr;

    CHECK("cria configuração temporária", write_temp_file(path, sizeof(path), config) == 0);
    CHECK("cria configuração inválida", write_temp_file(bad_path, sizeof(bad_path), malformed) == 0);

    overlay_init(&overlay);
    overlay_set_dim_alpha(&overlay, 0.0);
    CHECK("carrega arquivo JSON", overlay_load_file(&overlay, path) == 0);
    CHECK("overlay fica habilitado", overlay.enabled == 1);
    CHECK("assets_dir é carregado", overlay.assets_dir && strcmp(overlay.assets_dir, ".") == 0);
    CHECK("dois elementos foram carregados", overlay.elements->len == 2);
    baseline = find_element(&overlay, "baseline");
    caption = find_element(&overlay, "caption");
    CHECK("linha e suas coordenadas foram carregadas",
          baseline && baseline->type == OVERLAY_LINE && baseline->x1 == 8 &&
          baseline->y1 == 8 && baseline->x2 == 24 && baseline->y2 == 8);
    CHECK("label e template foram carregados",
          caption && caption->type == OVERLAY_LABEL &&
          strcmp(caption->text, "value={value}") == 0);

    CHECK("arquivo com assets_dir inválido é rejeitado", overlay_load_file(&overlay, bad_path) < 0);
    CHECK("falha de carga preserva elementos existentes", overlay.elements->len == 2 &&
          find_element(&overlay, "baseline") != NULL);

    overlay_apply_json_line(&overlay, "{\"type\":\"telemetry\",\"value\":\"42\"}");
    CHECK("telemetria atualiza variável", strcmp(g_hash_table_lookup(overlay.vars, "value"), "42") == 0);

    overlay_apply_json_line(&overlay,
        "{\"type\":\"set\",\"id\":\"baseline\",\"visible\":false,\"x\":17,\"alpha\":0.5}");
    baseline = find_element(&overlay, "baseline");
    CHECK("set altera visibilidade, posição e alpha", baseline && !baseline->visible &&
          baseline->x == 17 && baseline->alpha == 0.5);

    overlay_apply_json_line(&overlay,
        "{\"type\":\"upsert\",\"id\":\"dynamic\",\"element\":{"
        "\"type\":\"line\",\"x1\":8,\"y1\":20,\"x2\":24,\"y2\":20,\"width\":3}}");
    CHECK("upsert adiciona novo elemento", overlay.elements->len == 3 &&
          find_element(&overlay, "dynamic") != NULL);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 40, 40);
    cr = cairo_create(surface);
    clear_surface(cr);
    overlay_draw(&overlay, cr, 0, 0, 40, 40);
    CHECK("renderiza linha em surface sem display", count_green_pixels(surface) > 0);
    CHECK("renderiza texto com valor de telemetria",
          count_green_pixels(surface) > 10);

    clear_surface(cr);
    g_mutex_lock(&overlay.lock);
    overlay.enabled = 0;
    g_mutex_unlock(&overlay.lock);
    overlay_draw(&overlay, cr, 0, 0, 40, 40);
    CHECK("overlay desabilitado não desenha", count_green_pixels(surface) == 0);
    g_mutex_lock(&overlay.lock);
    overlay.enabled = 1;
    g_mutex_unlock(&overlay.lock);

    overlay_apply_json_line(&overlay, "{\"type\":\"delete\",\"id\":\"dynamic\"}");
    CHECK("delete remove elemento", overlay.elements->len == 2 &&
          find_element(&overlay, "dynamic") == NULL);

    overlay_apply_json_line(&overlay, "{\"type\":\"delete\",\"id\":\"caption\"}");
    clear_surface(cr);
    overlay_draw(&overlay, cr, 0, 0, 40, 40);
    CHECK("linha escondida não aparece após delete do label",
          count_green_pixels(surface) == 0);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    overlay_cleanup(&overlay);
    unlink(path);
    unlink(bad_path);
}

int main(void)
{
    test_load_json_updates_and_draw();
    if (failures) {
        fprintf(stderr, "%u de %u verificações de overlay falharam\n", failures, checks);
        return 1;
    }
    printf("%u verificações de overlay passaram\n", checks);
    return 0;
}
