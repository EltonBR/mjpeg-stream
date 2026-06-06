CC ?= cc
PKG_CONFIG ?= pkg-config

CFLAGS ?= -O2 -Wall -Wextra -pedantic
CPPFLAGS += -D_DEFAULT_SOURCE -Isrc/common

GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0 gdk-pixbuf-2.0 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk+-3.0 gdk-pixbuf-2.0 2>/dev/null)
JPEG_LIBS ?= -ljpeg

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
	$(COMMON_OBJS)
DISCOVER_OBJS = $(BUILD_DIR)/discover/main.o

.PHONY: all clean static-tx

all: mjpeg_tx mjpeg_rx v4l2_discover

mjpeg_tx: $(TX_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(JPEG_LIBS)

mjpeg_rx: $(RX_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(GTK_LIBS)

v4l2_discover: $(DISCOVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

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

$(BUILD_DIR)/rx/main.o: src/rx/main.c src/rx/app.h src/rx/config.h src/rx/event_sender.h src/rx/input.h src/rx/receiver.h src/rx/ui.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/config.o: src/rx/config.c src/rx/config.h src/common/ini.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/app.o: src/rx/app.c src/rx/app.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/ui.o: src/rx/ui.c src/rx/ui.h src/rx/app.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/receiver.o: src/rx/receiver.c src/rx/receiver.h src/rx/app.h src/rx/config.h src/rx/ui.h src/common/net.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/event_sender.o: src/rx/event_sender.c src/rx/event_sender.h src/common/net.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/input.o: src/rx/input.c src/rx/input.h src/rx/app.h src/rx/event_sender.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/discover/main.o: src/discover/main.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/common/net.o: src/common/net.c src/common/net.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/common/ini.o: src/common/ini.c src/common/ini.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf mjpeg_tx mjpeg_rx v4l2_discover $(BUILD_DIR) src/*.o
