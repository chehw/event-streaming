TARGET=event-streaming

DEBUG ?= 1
OPTIMIZE ?= -O2

# default webserver library
WEBSERVER_LIB ?= libsoup-2.4

#default event-streaming library
EVENT_STREAMING_LIB ?= rdkafka

#default json web token library
JSON_WEB_TOKEN_LIB ?= libjwt

CC=gcc -std=gnu99 -D_DEFAULT_SOURCE -D_GNU_SOURCE -Wall
LINKER=$(CC)

CFLAGS=-Iinclude -Iutils
LIBS=-lm -lpthread -lpcre -ljson-c

ifeq ($(DEBUG),1)
CFLAGS += -g -D_DEBUG
OPTIMIZE = -O0
endif

# libsoup
ifeq ($(WEBSERVER_LIB),libsoup-2.4)
CFLAGS += $(shell pkg-config --cflags libsoup-2.4)
LIBS   += $(shell pkg-config --libs   libsoup-2.4)
endif

# librdkafka
ifeq ($(EVENT_STREAMING_LIB),rdkafka)
CFLAGS += $(shell pkg-config --cflags rdkafka)
LIBS   += $(shell pkg-config --libs   rdkafka)
endif

# libjwt
ifeq ($(JSON_WEB_TOKEN_LIB),libjwt)
CFLAGS += $(shell pkg-config --cflags libjwt)
LIBS   += $(shell pkg-config --libs   libjwt)
endif

SRC_DIR=src
OBJ_DIR=obj
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

all: do_init $(TARGET)
$(TARGET): $(OBJECTS) $(UTILS_OBJECTS)
	$(LINKER) $(OPTIMIZE) -o $@ $^ $(LIBS)

$(OBJECTS): $(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	@ echo "$(CC) -o $@ -c $< $(CFLAGS)"
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: do_init clean
do_init:
	@ ./pre-build.sh || exit 1

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)


