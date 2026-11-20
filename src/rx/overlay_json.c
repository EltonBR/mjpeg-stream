#include "overlay_internal.h"

#include <json-c/json.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int json_get_obj(struct json_object *obj, const char *key,
                        struct json_object **out)
{
    return json_object_object_get_ex(obj, key, out);
}

static const char *json_get_string_default(struct json_object *obj,
                                           const char *key,
                                           const char *fallback)
{
    struct json_object *value;
    if (!json_get_obj(obj, key, &value) || json_object_get_type(value) != json_type_string) {
        return fallback;
    }
    return json_object_get_string(value);
}

static double json_get_double_default(struct json_object *obj, const char *key,
                                      double fallback)
{
    struct json_object *value;
    if (!json_get_obj(obj, key, &value)) {
        return fallback;
    }
    if (json_object_get_type(value) != json_type_double &&
        json_object_get_type(value) != json_type_int) {
        return fallback;
    }
    return json_object_get_double(value);
}

static int json_get_int_default(struct json_object *obj, const char *key,
                                int fallback)
{
    struct json_object *value;
    if (!json_get_obj(obj, key, &value) || json_object_get_type(value) != json_type_int) {
        return fallback;
    }
    return json_object_get_int(value);
}

static int json_get_bool_default(struct json_object *obj, const char *key,
                                 int fallback)
{
    struct json_object *value;
    if (!json_get_obj(obj, key, &value) || json_object_get_type(value) != json_type_boolean) {
        return fallback;
    }
    return json_object_get_boolean(value);
}

static enum overlay_anchor parse_anchor(const char *anchor)
{
    if (strcmp(anchor, "top-right") == 0) {
        return OVERLAY_ANCHOR_TOP_RIGHT;
    }
    if (strcmp(anchor, "bottom-left") == 0) {
        return OVERLAY_ANCHOR_BOTTOM_LEFT;
    }
    if (strcmp(anchor, "bottom-right") == 0) {
        return OVERLAY_ANCHOR_BOTTOM_RIGHT;
    }
    if (strcmp(anchor, "center") == 0) {
        return OVERLAY_ANCHOR_CENTER;
    }
    return OVERLAY_ANCHOR_TOP_LEFT;
}

int overlay_parse_color(const char *text, double *r, double *g, double *b)
{
    unsigned int rv;
    unsigned int gv;
    unsigned int bv;
    if (!text || strlen(text) != 7 || text[0] != '#') {
        return -1;
    }
    if (sscanf(text + 1, "%02x%02x%02x", &rv, &gv, &bv) != 3) {
        return -1;
    }
    *r = rv / 255.0;
    *g = gv / 255.0;
    *b = bv / 255.0;
    return 0;
}

static char *dup_json_string(struct json_object *obj, const char *key)
{
    struct json_object *value;
    if (!json_get_obj(obj, key, &value) || json_object_get_type(value) != json_type_string) {
        return NULL;
    }
    return strdup(json_object_get_string(value));
}

static int parse_rotation(struct overlay_element *el, struct json_object *obj)
{
    struct json_object *value;
    if (!json_get_obj(obj, "rotation", &value)) {
        el->rotation = 0.0;
        return 0;
    }
    if (json_object_get_type(value) == json_type_string) {
        el->rotation_expr = strdup(json_object_get_string(value));
        return el->rotation_expr ? 0 : -1;
    }
    if (json_object_get_type(value) == json_type_double ||
        json_object_get_type(value) == json_type_int) {
        el->rotation = json_object_get_double(value);
        return 0;
    }
    return -1;
}

static int valid_image_path(const char *file)
{
    if (!file || !file[0] || file[0] == '/' || strstr(file, "..")) {
        return 0;
    }
    return 1;
}

static struct overlay_element *parse_element(struct json_object *obj,
                                             const char *fallback_id)
{
    struct overlay_element *el;
    const char *type;
    const char *id;
    const char *color;

    if (json_object_get_type(obj) != json_type_object) {
        fprintf(stderr, "overlay: elemento invalido ignorado\n");
        return NULL;
    }

    type = json_get_string_default(obj, "type", NULL);
    if (!type) {
        fprintf(stderr, "overlay: elemento sem type ignorado\n");
        return NULL;
    }

    el = calloc(1, sizeof(*el));
    if (!el) {
        return NULL;
    }

    id = json_get_string_default(obj, "id", fallback_id);
    el->id = id ? strdup(id) : NULL;
    el->x = json_get_double_default(obj, "x", 0.0);
    el->y = json_get_double_default(obj, "y", 0.0);
    el->w = json_get_double_default(obj, "w", 0.0);
    el->h = json_get_double_default(obj, "h", 0.0);
    el->x1 = json_get_double_default(obj, "x1", 0.0);
    el->y1 = json_get_double_default(obj, "y1", 0.0);
    el->x2 = json_get_double_default(obj, "x2", 0.0);
    el->y2 = json_get_double_default(obj, "y2", 0.0);
    el->width = json_get_double_default(obj, "width", 1.0);
    el->alpha = json_get_double_default(obj, "alpha", 1.0);
    el->visible = json_get_bool_default(obj, "visible", 1);
    el->z_index = json_get_int_default(obj, "z_index", 0);
    el->anchor = parse_anchor(json_get_string_default(obj, "anchor", "top-left"));
    el->color_r = 1.0;
    el->color_g = 1.0;
    el->color_b = 1.0;

    if (el->alpha < 0.0) {
        el->alpha = 0.0;
    } else if (el->alpha > 1.0) {
        el->alpha = 1.0;
    }

    color = json_get_string_default(obj, "color", NULL);
    if (color && overlay_parse_color(color, &el->color_r, &el->color_g, &el->color_b) < 0) {
        fprintf(stderr, "overlay: cor invalida em %s, usando branco\n",
                el->id ? el->id : "(sem id)");
    }

    if (strcmp(type, "image") == 0) {
        el->type = OVERLAY_IMAGE;
        el->file = dup_json_string(obj, "file");
        if (!valid_image_path(el->file) || parse_rotation(el, obj) < 0) {
            fprintf(stderr, "overlay: image invalido ignorado: %s\n",
                    el->id ? el->id : "(sem id)");
            overlay_element_free(el);
            return NULL;
        }
    } else if (strcmp(type, "label") == 0) {
        el->type = OVERLAY_LABEL;
        el->text = dup_json_string(obj, "text");
        el->width = json_get_double_default(obj, "size", 16.0);
        if (!el->text) {
            fprintf(stderr, "overlay: label sem text ignorado\n");
            overlay_element_free(el);
            return NULL;
        }
    } else if (strcmp(type, "line") == 0) {
        el->type = OVERLAY_LINE;
        if (parse_rotation(el, obj) < 0) {
            fprintf(stderr, "overlay: line com rotation invalido ignorado: %s\n",
                    el->id ? el->id : "(sem id)");
            overlay_element_free(el);
            return NULL;
        }
    } else {
        fprintf(stderr, "overlay: type desconhecido ignorado: %s\n", type);
        overlay_element_free(el);
        return NULL;
    }

    return el;
}

int overlay_load_file(struct overlay_state *overlay, const char *path)
{
    struct json_object *root;
    struct json_object *elements;
    struct json_object *widgets;
    const char *assets_dir;
    size_t i;

    root = json_object_from_file(path);
    if (!root) {
        fprintf(stderr, "overlay: nao foi possivel ler %s\n", path);
        return -1;
    }
    if (json_object_get_type(root) != json_type_object) {
        fprintf(stderr, "overlay: raiz JSON invalida em %s\n", path);
        json_object_put(root);
        return -1;
    }

    assets_dir = json_get_string_default(root, "assets_dir", ".");
    if (assets_dir[0] == '/' || strstr(assets_dir, "..")) {
        fprintf(stderr, "overlay: assets_dir invalido: %s\n", assets_dir);
        json_object_put(root);
        return -1;
    }

    json_get_obj(root, "elements", &elements);
    json_get_obj(root, "widgets", &widgets);
    if ((!elements || json_object_get_type(elements) != json_type_array) &&
        (!widgets || json_object_get_type(widgets) != json_type_array)) {
        fprintf(stderr, "overlay: elements ou widgets precisa ser array\n");
        json_object_put(root);
        return -1;
    }

    g_mutex_lock(&overlay->lock);
    free(overlay->assets_dir);
    overlay->assets_dir = strdup(assets_dir);
    g_ptr_array_set_size(overlay->elements, 0);

    if (elements && json_object_get_type(elements) == json_type_array) {
        for (i = 0; i < json_object_array_length(elements); i++) {
            struct overlay_element *el =
                parse_element(json_object_array_get_idx(elements, i), NULL);
            if (el) {
                g_ptr_array_add(overlay->elements, el);
            }
        }
    }
    overlay_widgets_load(overlay->widgets, widgets);
    overlay->enabled = 1;
    g_mutex_unlock(&overlay->lock);

    json_object_put(root);
    return 0;
}

static struct overlay_element *find_element_locked(struct overlay_state *overlay,
                                                   const char *id,
                                                   unsigned *index)
{
    unsigned i;
    if (!id) {
        return NULL;
    }
    for (i = 0; i < overlay->elements->len; i++) {
        struct overlay_element *el = g_ptr_array_index(overlay->elements, i);
        if (el->id && strcmp(el->id, id) == 0) {
            if (index) {
                *index = i;
            }
            return el;
        }
    }
    return NULL;
}

static void apply_set(struct overlay_state *overlay, struct json_object *root)
{
    struct overlay_element *el;
    const char *id = json_get_string_default(root, "id", NULL);
    struct json_object *value;

    el = find_element_locked(overlay, id, NULL);
    if (!el) {
        fprintf(stderr, "overlay: set ignorado, id nao encontrado: %s\n",
                id ? id : "(null)");
        return;
    }

    if (json_get_obj(root, "visible", &value) &&
        json_object_get_type(value) == json_type_boolean) {
        el->visible = json_object_get_boolean(value);
    }
    if (json_get_obj(root, "rotation", &value)) {
        free(el->rotation_expr);
        el->rotation_expr = NULL;
        if (json_object_get_type(value) == json_type_string) {
            el->rotation_expr = strdup(json_object_get_string(value));
        } else if (json_object_get_type(value) == json_type_double ||
                   json_object_get_type(value) == json_type_int) {
            el->rotation = json_object_get_double(value);
        }
    }
    if (json_get_obj(root, "x", &value)) {
        el->x = json_object_get_double(value);
    }
    if (json_get_obj(root, "y", &value)) {
        el->y = json_object_get_double(value);
    }
    if (json_get_obj(root, "alpha", &value)) {
        el->alpha = json_object_get_double(value);
    }
}

static void apply_telemetry(struct overlay_state *overlay,
                            struct json_object *root)
{
    json_object_object_foreach(root, key, value) {
        const char *stored;
        if (strcmp(key, "type") == 0) {
            continue;
        }
        if (json_object_get_type(value) == json_type_double ||
            json_object_get_type(value) == json_type_int) {
            stored = json_object_get_string(value);
        } else if (json_object_get_type(value) == json_type_string) {
            stored = json_object_get_string(value);
        } else if (json_object_get_type(value) == json_type_boolean) {
            stored = json_object_get_boolean(value) ? "true" : "false";
        } else {
            continue;
        }
        g_hash_table_replace(overlay->vars, g_strdup(key), g_strdup(stored));
    }
}

static void apply_upsert(struct overlay_state *overlay, struct json_object *root)
{
    struct json_object *element_obj;
    struct overlay_element *el;
    const char *id = json_get_string_default(root, "id", NULL);
    unsigned index = 0;

    if (!id || !json_get_obj(root, "element", &element_obj)) {
        fprintf(stderr, "overlay: upsert invalido ignorado\n");
        return;
    }
    el = parse_element(element_obj, id);
    if (!el) {
        return;
    }
    free(el->id);
    el->id = strdup(id);

    if (find_element_locked(overlay, id, &index)) {
        overlay_element_free(g_ptr_array_index(overlay->elements, index));
        g_ptr_array_index(overlay->elements, index) = el;
    } else {
        g_ptr_array_add(overlay->elements, el);
    }
    overlay->enabled = 1;
}

static void apply_delete(struct overlay_state *overlay, struct json_object *root)
{
    const char *id = json_get_string_default(root, "id", NULL);
    unsigned index;
    if (find_element_locked(overlay, id, &index)) {
        g_ptr_array_remove_index(overlay->elements, index);
    }
}

void overlay_apply_json_line(struct overlay_state *overlay, const char *line)
{
    struct json_object *root = json_tokener_parse(line);
    const char *type;

    if (!root || json_object_get_type(root) != json_type_object) {
        fprintf(stderr, "overlay: JSON de telemetria invalido ignorado\n");
        if (root) {
            json_object_put(root);
        }
        return;
    }

    type = json_get_string_default(root, "type", NULL);
    if (!type) {
        fprintf(stderr, "overlay: mensagem sem type ignorada\n");
        json_object_put(root);
        return;
    }

    g_mutex_lock(&overlay->lock);
    if (strcmp(type, "telemetry") == 0) {
        apply_telemetry(overlay, root);
    } else if (strcmp(type, "set") == 0) {
        apply_set(overlay, root);
    } else if (strcmp(type, "upsert") == 0) {
        apply_upsert(overlay, root);
    } else if (strcmp(type, "delete") == 0) {
        apply_delete(overlay, root);
    } else {
        fprintf(stderr, "overlay: type de mensagem desconhecido: %s\n", type);
    }
    g_mutex_unlock(&overlay->lock);

    json_object_put(root);
}
