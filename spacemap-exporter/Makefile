all: spacemapExporter

CFLAGS += -Wall -Wunused
CFLAGS += $(shell pkg-config --cflags libzfs)
LDFLAGS += $(shell pkg-config --libs libzfs) -lzpool

spacemapExporter: spacemapExporter.c

install: spacemapExporter
	mkdir -pv ${out}/bin
	cp -v spacemapExporter ${out}/bin/
