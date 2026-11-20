#include "overlay_internal.h"

#include <json-c/json.h>
#include <math.h>

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
    if (sscanf(text + 1, "%02x%02x%02x", &rv, &gv, &bv) != 3 ||
        strlen(text) != 7 || text[0] != '#' ||
        strspn(text + 1, "0123456789abcdefABCDEF") != 6) {
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
        return isfinite(el->rotation) ? 0 : -1;
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

    {
        const char *number_keys[] = {"x", "y", "w", "h", "x1", "y1", "x2",
                                     "y2", "width", "size", "alpha"};
        const char *string_keys[] = {"id", "type", "file", "text", "color", "anchor"};
        size_t i;
        struct json_object *value;
        for (i = 0; i < G_N_ELEMENTS(number_keys); i++) {
            enum json_type kind;
            if (!json_object_object_get_ex(obj, number_keys[i], &value)) continue;
            kind = json_object_get_type(value);
            if ((kind != json_type_int && kind != json_type_double) ||
                !isfinite(json_object_get_double(value))) return NULL;
        }
        for (i = 0; i < G_N_ELEMENTS(string_keys); i++) {
            if (json_object_object_get_ex(obj, string_keys[i], &value) &&
                json_object_get_type(value) != json_type_string) return NULL;
        }
        if (json_object_object_get_ex(obj, "visible", &value) &&
            json_object_get_type(value) != json_type_boolean) return NULL;
        if (json_object_object_get_ex(obj, "z_index", &value) &&
            json_object_get_type(value) != json_type_int) return NULL;
        if (json_object_object_get_ex(obj, "rotation", &value) &&
            json_object_get_type(value) != json_type_string &&
            json_object_get_type(value) != json_type_double &&
            json_object_get_type(value) != json_type_int) return NULL;
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
        overlay_element_free(el);
        return NULL;
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
    struct json_object *root = NULL, *elements = NULL, *widgets = NULL;
    const char *assets_dir;
    char *new_assets = NULL;
    GPtrArray *new_elements = NULL, *new_widgets = NULL;
    GHashTable *ids = NULL;
    size_t i;
    int result = -1;

    root = json_object_from_file(path);
    if (!root || json_object_get_type(root) != json_type_object) goto out;
    {
        struct json_object *version;
        if (json_object_object_get_ex(root, "schema_version", &version) &&
            (json_object_get_type(version) != json_type_int ||
             json_object_get_int(version) != 2)) goto out;
    }
    {
        struct json_object *assets;
        if (json_object_object_get_ex(root, "assets_dir", &assets) &&
            json_object_get_type(assets) != json_type_string) goto out;
    }
    assets_dir = json_get_string_default(root, "assets_dir", ".");
    if (assets_dir[0] == '/' || strstr(assets_dir, "..")) goto out;
    json_get_obj(root, "elements", &elements);
    json_get_obj(root, "widgets", &widgets);
    if ((!elements && !widgets) ||
        (elements && json_object_get_type(elements) != json_type_array) ||
        (widgets && json_object_get_type(widgets) != json_type_array)) goto out;
    g_mutex_lock(&overlay->lock);
    if (overlay->assets_override)
        new_assets = strdup(overlay->assets_override);
    else if (overlay->assets_base)
        new_assets = g_build_filename(overlay->assets_base, assets_dir, NULL);
    else
        new_assets = strdup(assets_dir);
    g_mutex_unlock(&overlay->lock);
    new_elements = g_ptr_array_new_with_free_func(overlay_element_free);
    new_widgets = overlay_widgets_new();
    ids = g_hash_table_new(g_str_hash, g_str_equal);
    if (!new_assets) goto out;
    if (elements) {
        for (i = 0; i < json_object_array_length(elements); i++) {
            struct overlay_element *el = parse_element(json_object_array_get_idx(elements, i), NULL);
            if (!el) goto out;
            g_ptr_array_add(new_elements, el);
            if (el->id && *el->id) {
                if (g_hash_table_contains(ids, el->id)) goto out;
                g_hash_table_add(ids, el->id);
            }
        }
    }
    if (overlay_widgets_load(new_widgets, widgets) < 0) goto out;
    for (i = 0; i < new_widgets->len; i++) {
        struct overlay_widget *widget = g_ptr_array_index(new_widgets, i);
        if (widget->id && *widget->id) {
            if (g_hash_table_contains(ids, widget->id)) goto out;
            g_hash_table_add(ids, widget->id);
        }
    }
    g_mutex_lock(&overlay->lock);
    {
        GPtrArray *old_elements = overlay->elements;
        GPtrArray *old_widgets = overlay->widgets;
        char *old_assets = overlay->assets_dir;
        overlay->elements = new_elements;
        overlay->widgets = new_widgets;
        overlay->assets_dir = new_assets;
        overlay->enabled = 1;
        new_elements = old_elements;
        new_widgets = old_widgets;
        new_assets = old_assets;
    }
    g_mutex_unlock(&overlay->lock);
    result = 0;
out:
    if (result < 0) fprintf(stderr, "overlay: layout invalido: %s\n", path);
    if (ids) g_hash_table_destroy(ids);
    if (new_elements) g_ptr_array_free(new_elements, TRUE);
    if (new_widgets) g_ptr_array_free(new_widgets, TRUE);
    free(new_assets);
    if (root) json_object_put(root);
    return result;
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

static struct overlay_widget *clone_widget(const struct overlay_widget *source)
{
    struct overlay_widget *copy = malloc(sizeof(*copy));
    unsigned i;
    if (!copy) return NULL;
    *copy = *source;
    copy->id = copy->file = copy->text = copy->azimuth_expr = NULL;
    copy->rotation_expr = copy->value_expr = copy->label = copy->suffix = NULL;
    copy->items = NULL;
    copy->pixbuf = source->pixbuf ? g_object_ref(source->pixbuf) : NULL;
    for (i = 0; i < G_N_ELEMENTS(copy->compass_labels); i++)
        copy->compass_labels[i] = NULL;
#define DUP_FIELD(field) do { \
    if (source->field && !(copy->field = strdup(source->field))) goto fail; \
} while (0)
    DUP_FIELD(id); DUP_FIELD(file); DUP_FIELD(text); DUP_FIELD(azimuth_expr);
    DUP_FIELD(rotation_expr); DUP_FIELD(value_expr); DUP_FIELD(label); DUP_FIELD(suffix);
#undef DUP_FIELD
    for (i = 0; i < G_N_ELEMENTS(copy->compass_labels); i++) {
        if (source->compass_labels[i] &&
            !(copy->compass_labels[i] = strdup(source->compass_labels[i]))) goto fail;
    }
    if (source->items) {
        copy->items = g_ptr_array_new_with_free_func(readout_item_free);
        for (i = 0; i < source->items->len; i++) {
            const struct readout_item *old = g_ptr_array_index(source->items, i);
            struct readout_item *item = calloc(1, sizeof(*item));
            if (!item) goto fail;
            item->label = old->label ? strdup(old->label) : NULL;
            item->value = old->value ? strdup(old->value) : NULL;
            if ((old->label && !item->label) || (old->value && !item->value)) {
                readout_item_free(item);
                goto fail;
            }
            g_ptr_array_add(copy->items, item);
        }
    }
    return copy;
fail:
    overlay_widget_free(copy);
    return NULL;
}

static int json_number(struct json_object *obj, const char *key,
                       struct json_object **out, double *number)
{
    enum json_type type;
    if (!json_object_object_get_ex(obj, key, out)) return 0;
    type = json_object_get_type(*out);
    if (type != json_type_int && type != json_type_double) return -1;
    *number = json_object_get_double(*out);
    return isfinite(*number) ? 1 : -1;
}

static int replace_widget_array(struct overlay_widget *widget,
                                struct json_object *root, const char *key)
{
    struct json_object *array;
    GPtrArray *replacement;
    size_t i;
    if (!json_object_object_get_ex(root, key, &array)) return 0;
    if (json_object_get_type(array) != json_type_array) return -1;
    if (strcmp(key, "items") == 0 && widget->type != WIDGET_READOUT) return -1;
    if (strcmp(key, "labels") == 0 && widget->type != WIDGET_COMPASS) return -1;
    replacement = g_ptr_array_new_with_free_func(readout_item_free);
    for (i = 0; i < json_object_array_length(array); i++) {
        struct json_object *entry = json_object_array_get_idx(array, i);
        if (strcmp(key, "items") == 0) {
            struct json_object *label, *value;
            struct readout_item *item;
            if (json_object_get_type(entry) != json_type_object ||
                !json_object_object_get_ex(entry, "label", &label) ||
                !json_object_object_get_ex(entry, "value", &value) ||
                json_object_get_type(label) != json_type_string ||
                json_object_get_type(value) != json_type_string) goto invalid;
            item = calloc(1, sizeof(*item));
            if (!item) goto invalid;
            item->label = strdup(json_object_get_string(label));
            item->value = strdup(json_object_get_string(value));
            if (!item->label || !item->value) {
                readout_item_free(item);
                goto invalid;
            }
            g_ptr_array_add(replacement, item);
        } else {
            if (json_object_get_type(entry) != json_type_string || i >= 8)
                goto invalid;
        }
    }
    if (strcmp(key, "labels") == 0 && json_object_array_length(array) != 8)
        goto invalid;
    if (strcmp(key, "items") == 0) {
        if (widget->items) g_ptr_array_free(widget->items, TRUE);
        widget->items = replacement;
    } else {
        for (i = 0; i < 8; i++) {
            struct json_object *entry = json_object_array_get_idx(array, i);
            char *label = strdup(json_object_get_string(entry));
            if (!label) goto invalid;
            free(widget->compass_labels[i]);
            widget->compass_labels[i] = label;
        }
        g_ptr_array_free(replacement, TRUE);
    }
    return 0;
invalid:
    g_ptr_array_free(replacement, TRUE);
    return -1;
}

static int apply_widget_set(struct overlay_state *overlay,
                            struct overlay_widget *widget,
                            struct json_object *root)
{
    struct overlay_widget *copy = clone_widget(widget);
    struct json_object *value;
    double number;
    const char *number_keys[] = {"x", "y", "w", "h", "alpha", "size", "width",
                                 "min", "max", "window", "major_step", "minor_step"};
    double *number_fields[ G_N_ELEMENTS(number_keys) ];
    size_t i;
    if (!copy) return -1;
    number_fields[0] = &copy->x; number_fields[1] = &copy->y;
    number_fields[2] = &copy->w; number_fields[3] = &copy->h;
    number_fields[4] = &copy->alpha; number_fields[5] = &copy->size;
    number_fields[6] = &copy->width; number_fields[7] = &copy->min_value;
    number_fields[8] = &copy->max_value; number_fields[9] = &copy->window;
    number_fields[10] = &copy->major_step; number_fields[11] = &copy->minor_step;
    for (i = 0; i < G_N_ELEMENTS(number_keys); i++) {
        int found = json_number(root, number_keys[i], &value, &number);
        if (found < 0) goto invalid;
        if (!found) continue;
        if ((i >= 2 && i <= 3 && number < 0) || (i == 4 && (number < 0 || number > 1)) ||
            ((i == 5 || i == 6 || i >= 9) && number <= 0)) goto invalid;
        *number_fields[i] = number;
    }
    if ((copy->type == WIDGET_VERTICAL_RULER ||
         copy->type == WIDGET_HORIZONTAL_RULER) &&
        (copy->min_value >= copy->max_value || copy->window <= 0 ||
         copy->major_step <= 0 || copy->minor_step <= 0)) goto invalid;
    if (json_object_object_get_ex(root, "z_index", &value)) {
        if (json_object_get_type(value) != json_type_int) goto invalid;
        copy->z_index = json_object_get_int(value);
    }
    {
        const char *bool_keys[] = {"visible", "flip_horizontal", "flip_vertical"};
        int *fields[] = {&copy->visible, &copy->flip_horizontal, &copy->flip_vertical};
        for (i = 0; i < G_N_ELEMENTS(bool_keys); i++) {
            if (!json_object_object_get_ex(root, bool_keys[i], &value)) continue;
            if (json_object_get_type(value) != json_type_boolean) goto invalid;
            *fields[i] = json_object_get_boolean(value);
        }
    }
    {
        const char *unit_keys[] = {"x_unit", "y_unit", "w_unit", "h_unit"};
        enum widget_coord_unit *fields[] = {&copy->x_unit, &copy->y_unit,
                                            &copy->w_unit, &copy->h_unit};
        for (i = 0; i < G_N_ELEMENTS(unit_keys); i++) {
            const char *unit;
            if (!json_object_object_get_ex(root, unit_keys[i], &value)) continue;
            if (json_object_get_type(value) != json_type_string) goto invalid;
            unit = json_object_get_string(value);
            if (strcmp(unit, "relative") == 0) *fields[i] = WIDGET_COORD_RELATIVE;
            else if (strcmp(unit, "px") == 0) *fields[i] = WIDGET_COORD_PX;
            else goto invalid;
        }
    }
    {
        const char *keys[] = {"file", "text", "value", "azimuth", "rotation", "label", "suffix"};
        char **fields[] = {&copy->file, &copy->text, &copy->value_expr, &copy->azimuth_expr,
                           &copy->rotation_expr, &copy->label, &copy->suffix};
        for (i = 0; i < G_N_ELEMENTS(keys); i++) {
            char *replacement;
            if (!json_object_object_get_ex(root, keys[i], &value)) continue;
            if (json_object_get_type(value) != json_type_string) goto invalid;
            replacement = strdup(json_object_get_string(value));
            if (!replacement) goto invalid;
            free(*fields[i]);
            *fields[i] = replacement;
        }
    }
    if (json_object_object_get_ex(root, "anchor", &value)) {
        const char *anchors[] = {"top-left", "top-center", "top-right", "bottom-left",
                                 "bottom-right", "center"};
        if (json_object_get_type(value) != json_type_string) goto invalid;
        for (i = 0; i < G_N_ELEMENTS(anchors) &&
             strcmp(json_object_get_string(value), anchors[i]) != 0; i++);
        if (i == G_N_ELEMENTS(anchors)) goto invalid;
        copy->anchor = (enum widget_anchor)i;
    }
    if (replace_widget_array(copy, root, "items") < 0 ||
        replace_widget_array(copy, root, "labels") < 0) goto invalid;
    if (copy->type == WIDGET_IMAGE && !widget_valid_image_path(copy->file)) goto invalid;
    {
        struct overlay_widget *old = widget;
        unsigned index;
        for (index = 0; index < overlay->widgets->len; index++)
            if (g_ptr_array_index(overlay->widgets, index) == old) break;
        if (index == overlay->widgets->len) goto invalid;
        g_ptr_array_index(overlay->widgets, index) = copy;
        overlay_widget_free(old);
    }
    return 0;
invalid:
    overlay_widget_free(copy);
    return -1;
}

static void apply_set(struct overlay_state *overlay, struct json_object *root)
{
    struct overlay_element *el;
    const char *id = json_get_string_default(root, "id", NULL);
    struct json_object *value;

    el = find_element_locked(overlay, id, NULL);
    if (!el) {
        unsigned i;
        for (i = 0; id && i < overlay->widgets->len; i++) {
            struct overlay_widget *widget = g_ptr_array_index(overlay->widgets, i);
            if (!widget->id || strcmp(widget->id, id) != 0) continue;
            if (apply_widget_set(overlay, widget, root) < 0)
                fprintf(stderr, "overlay: set de widget invalido ignorado: %s\n", id);
            return;
        }
        fprintf(stderr, "overlay: set ignorado, id nao encontrado: %s\n",
                id ? id : "(null)");
        return;
    }

    {
        double x = el->x, y = el->y, alpha = el->alpha, rotation = el->rotation;
        int visible = el->visible;
        char *rotation_expr = NULL;
        struct json_object *rotation_value = NULL;
        int has_rotation = json_get_obj(root, "rotation", &rotation_value);
        if (json_get_obj(root, "visible", &value)) {
            if (json_object_get_type(value) != json_type_boolean) return;
            visible = json_object_get_boolean(value);
        }
        if (json_number(root, "x", &value, &x) < 0 ||
            json_number(root, "y", &value, &y) < 0 ||
            json_number(root, "alpha", &value, &alpha) < 0 || alpha < 0 || alpha > 1)
            return;
        if (has_rotation) {
            enum json_type kind = json_object_get_type(rotation_value);
            if (kind == json_type_string) {
                rotation_expr = strdup(json_object_get_string(rotation_value));
                if (!rotation_expr) return;
            } else if (kind == json_type_double || kind == json_type_int) {
                rotation = json_object_get_double(rotation_value);
                if (!isfinite(rotation)) return;
            } else return;
        }
        if (has_rotation) {
            free(el->rotation_expr);
            el->rotation_expr = rotation_expr;
            el->rotation = rotation;
        }
        el->visible = visible;
        el->x = x;
        el->y = y;
        el->alpha = alpha;
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
    {
        unsigned i;
        for (i = 0; i < overlay->widgets->len; i++) {
            struct overlay_widget *widget = g_ptr_array_index(overlay->widgets, i);
            if (widget->id && strcmp(widget->id, id) == 0) return;
        }
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
