#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <json-c/json.h>

#include "rx/widgets/common/widget_common.h"
#include "rx/widgets/compass/compass_widget.h"
#include "rx/widgets/image/image_widget.h"
#include "rx/widgets/readout/readout_widget.h"
#include "rx/widgets/status/status_widget.h"
#include "rx/widgets/vertical_ruler/vertical_ruler_widget.h"

typedef struct overlay_widget *(*widget_parser)(struct json_object *);

static struct json_object *parse_json(const char *json)
{
    struct json_object *obj = json_tokener_parse(json);
    assert(obj != NULL);
    return obj;
}

static void check_widget_units(widget_parser parser, const char *type)
{
    char json[512];
    struct json_object *obj;
    struct overlay_widget *widget;

    snprintf(json, sizeof(json),
             "{\"id\":\"unit-test\",\"type\":\"%s\",\"file\":\"unit-test.png\","
             "\"x\":1,\"x_unit\":\"relative\","
             "\"y\":-0.25,\"y_unit\":\"px\","
             "\"w\":0.5,\"w_unit\":\"px\","
             "\"h\":1,\"h_unit\":\"relative\"}", type);
    obj = parse_json(json);
    widget = parser(obj);
    assert(widget != NULL);
    assert(widget->x_unit == WIDGET_COORD_RELATIVE);
    assert(widget->y_unit == WIDGET_COORD_PX);
    assert(widget->w_unit == WIDGET_COORD_PX);
    assert(widget->h_unit == WIDGET_COORD_RELATIVE);
    overlay_widget_free(widget);
    json_object_put(obj);
}

int main(void)
{
    struct json_object *obj;
    struct overlay_widget *widget;

    assert(fabs(widget_coord_value(1.0, WIDGET_COORD_RELATIVE, 640.0) - 640.0) < 0.0001);
    assert(fabs(widget_coord_value(-0.25, WIDGET_COORD_RELATIVE, 400.0) + 100.0) < 0.0001);
    assert(fabs(widget_coord_value(1.0, WIDGET_COORD_PX, 640.0) - 1.0) < 0.0001);
    assert(fabs(widget_coord_value(0.5, WIDGET_COORD_PX, 640.0) - 0.5) < 0.0001);

    /* Missing (or unsupported) units keep the original 0..1 heuristic. */
    assert(fabs(widget_coord_value(0.5, WIDGET_COORD_LEGACY, 640.0) - 320.0) < 0.0001);
    assert(fabs(widget_coord_value(1.0, WIDGET_COORD_LEGACY, 640.0) - 640.0) < 0.0001);
    assert(fabs(widget_coord_value(1.5, WIDGET_COORD_LEGACY, 640.0) - 1.5) < 0.0001);
    assert(fabs(widget_coord_value(-0.25, WIDGET_COORD_LEGACY, 400.0) + 0.25) < 0.0001);

    check_widget_units(compass_widget_parse, "compass");
    check_widget_units(readout_widget_parse, "readout");
    check_widget_units(status_widget_parse, "status");
    check_widget_units(image_widget_parse, "image");
    check_widget_units(vertical_ruler_widget_parse, "vertical_ruler");
    check_widget_units(horizontal_ruler_widget_parse, "horizontal_ruler");

    obj = parse_json("{\"id\":\"legacy\",\"type\":\"status\",\"x\":0.5}");
    widget = status_widget_parse(obj);
    assert(widget != NULL);
    assert(widget->x_unit == WIDGET_COORD_LEGACY);
    assert(widget->y_unit == WIDGET_COORD_LEGACY);
    assert(fabs(widget_coord_value(widget->x, widget->x_unit, 800.0) - 400.0) < 0.0001);
    overlay_widget_free(widget);
    json_object_put(obj);

    obj = parse_json("{\"id\":\"bad-unit\",\"type\":\"status\","
                     "\"x\":1,\"x_unit\":\"percent\"}");
    widget = status_widget_parse(obj);
    assert(widget != NULL);
    assert(widget->x_unit == WIDGET_COORD_LEGACY);
    assert(fabs(widget_coord_value(widget->x, widget->x_unit, 800.0) - 800.0) < 0.0001);
    overlay_widget_free(widget);
    json_object_put(obj);

    puts("widget coordinate units: ok");
    return 0;
}
