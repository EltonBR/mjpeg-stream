CC ?= cc
PKG_CONFIG ?= pkg-config

CFLAGS ?= -O2 -Wall -Wextra -pedantic
CPPFLAGS += -D_DEFAULT_SOURCE -Isrc/common

GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0 gdk-pixbuf-2.0 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk+-3.0 gdk-pixbuf-2.0 2>/dev/null)
JSON_CFLAGS := $(shell $(PKG_CONFIG) --cflags json-c 2>/dev/null)
JSON_LIBS := $(shell $(PKG_CONFIG) --libs json-c 2>/dev/null)
JPEG_LIBS ?= -ljpeg
MATH_LIBS ?= -lm

BUILD_DIR = build
COMMON_OBJS = $(BUILD_DIR)/common/net.o $(BUILD_DIR)/common/ini.o
TX_OBJS = \
	$(BUILD_DIR)/tx/main.o \
	$(BUILD_DIR)/tx/access_control.o \
	$(BUILD_DIR)/tx/config.o \
	$(BUILD_DIR)/tx/camera.o \
	$(BUILD_DIR)/tx/jpeg.o \
	$(BUILD_DIR)/tx/stream.o \
	$(COMMON_OBJS)
RX_OBJS = \
	$(BUILD_DIR)/rx/main.o \
	$(BUILD_DIR)/rx/config.o \
	$(BUILD_DIR)/rx/app.o \
	$(BUILD_DIR)/rx/ui.o \
	$(BUILD_DIR)/rx/receiver.o \
	$(BUILD_DIR)/rx/event_sender.o \
	$(BUILD_DIR)/rx/input.o \
	$(BUILD_DIR)/rx/overlay.o \
	$(BUILD_DIR)/rx/overlay_widgets.o \
	$(BUILD_DIR)/rx/widgets/common/widget_common.o \
	$(BUILD_DIR)/rx/widgets/compass/compass_widget.o \
	$(BUILD_DIR)/rx/widgets/readout/readout_widget.o \
	$(BUILD_DIR)/rx/widgets/status/status_widget.o \
	$(BUILD_DIR)/rx/widgets/image/image_widget.o \
	$(BUILD_DIR)/rx/widgets/vertical_ruler/vertical_ruler_widget.o \
	$(BUILD_DIR)/rx/telemetry.o \
	$(COMMON_OBJS)
DISCOVER_OBJS = $(BUILD_DIR)/discover/main.o
ASSET_COLORIZE_OBJS = $(BUILD_DIR)/tools/asset_colorize.o

.PHONY: all clean static-tx

all: mjpeg_tx mjpeg_rx v4l2_discover asset_colorize

mjpeg_tx: $(TX_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(JPEG_LIBS)

mjpeg_rx: $(RX_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(GTK_LIBS) $(JSON_LIBS) $(MATH_LIBS)

v4l2_discover: $(DISCOVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

asset_colorize: $(ASSET_COLORIZE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(GTK_LIBS)

static-tx: $(TX_OBJS)
	$(CC) $(CFLAGS) -static -o mjpeg_tx $^ $(JPEG_LIBS)

$(BUILD_DIR)/tx/main.o: src/tx/main.c src/tx/config.h src/tx/camera.h src/tx/jpeg.h src/tx/stream.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tx/access_control.o: src/tx/access_control.c src/tx/access_control.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tx/config.o: src/tx/config.c src/tx/config.h src/tx/access_control.h src/common/ini.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tx/camera.o: src/tx/camera.c src/tx/camera.h src/tx/config.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tx/jpeg.o: src/tx/jpeg.c src/tx/jpeg.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tx/stream.o: src/tx/stream.c src/tx/stream.h src/tx/config.h src/tx/access_control.h src/common/net.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/main.o: src/rx/main.c src/rx/app.h src/rx/config.h src/rx/event_sender.h src/rx/input.h src/rx/overlay.h src/rx/receiver.h src/rx/telemetry.h src/rx/ui.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/config.o: src/rx/config.c src/rx/config.h src/common/ini.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/app.o: src/rx/app.c src/rx/app.h src/rx/overlay.h src/rx/telemetry.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/ui.o: src/rx/ui.c src/rx/ui.h src/rx/app.h src/rx/overlay.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/receiver.o: src/rx/receiver.c src/rx/receiver.h src/rx/app.h src/rx/config.h src/rx/ui.h src/common/net.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/event_sender.o: src/rx/event_sender.c src/rx/event_sender.h src/common/net.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/input.o: src/rx/input.c src/rx/input.h src/rx/app.h src/rx/event_sender.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/overlay.o: src/rx/overlay.c src/rx/overlay.h src/rx/overlay_widgets.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/overlay_widgets.o: src/rx/overlay_widgets.c src/rx/overlay_widgets.h src/rx/widgets/common/widget_common.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/widgets/common/widget_common.o: src/rx/widgets/common/widget_common.c src/rx/widgets/common/widget_common.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/widgets/compass/compass_widget.o: src/rx/widgets/compass/compass_widget.c src/rx/widgets/compass/compass_widget.h src/rx/widgets/common/widget_common.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/widgets/readout/readout_widget.o: src/rx/widgets/readout/readout_widget.c src/rx/widgets/readout/readout_widget.h src/rx/widgets/common/widget_common.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/widgets/status/status_widget.o: src/rx/widgets/status/status_widget.c src/rx/widgets/status/status_widget.h src/rx/widgets/common/widget_common.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/widgets/image/image_widget.o: src/rx/widgets/image/image_widget.c src/rx/widgets/image/image_widget.h src/rx/widgets/common/widget_common.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/widgets/vertical_ruler/vertical_ruler_widget.o: src/rx/widgets/vertical_ruler/vertical_ruler_widget.c src/rx/widgets/vertical_ruler/vertical_ruler_widget.h src/rx/widgets/common/widget_common.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/telemetry.o: src/rx/telemetry.c src/rx/telemetry.h src/rx/overlay.h src/common/net.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/discover/main.o: src/discover/main.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tools/asset_colorize.o: src/tools/asset_colorize.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/common/net.o: src/common/net.c src/common/net.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/common/ini.o: src/common/ini.c src/common/ini.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf mjpeg_tx mjpeg_rx v4l2_discover asset_colorize $(BUILD_DIR) src/*.o
