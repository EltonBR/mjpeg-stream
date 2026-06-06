CC ?= cc
PKG_CONFIG ?= pkg-config

CFLAGS ?= -O2 -Wall -Wextra -pedantic
CPPFLAGS += -D_DEFAULT_SOURCE -Isrc/common

GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0 gdk-pixbuf-2.0 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk+-3.0 gdk-pixbuf-2.0 2>/dev/null)
JPEG_LIBS ?= -ljpeg

BUILD_DIR = build
COMMON_OBJS = $(BUILD_DIR)/common/net.o
TX_OBJS = $(BUILD_DIR)/tx/main.o $(COMMON_OBJS)
RX_OBJS = $(BUILD_DIR)/rx/main.o $(COMMON_OBJS)
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

$(BUILD_DIR)/tx/main.o: src/tx/main.c src/common/net.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rx/main.o: src/rx/main.c src/common/net.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/discover/main.o: src/discover/main.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/common/net.o: src/common/net.c src/common/net.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf mjpeg_tx mjpeg_rx v4l2_discover $(BUILD_DIR) src/*.o
