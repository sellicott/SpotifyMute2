.PHONY: all clean

PROJECT := spotify_mute

SRCS := main.c dbus_utils.c
INCLUDES := include

# source transformation
INCLUDES_ARG = $(INCLUDES:%=-I%)
OBJS := $(SRCS:%.c=%.o)
BIN_OBJS := $(OBJS:%.o=bin/%.o)

CC := gcc

DEBUG  := -ggdb3 -Og
CFLAGS := \
    -std=c99 -pedantic -Wall \
    -Wno-missing-braces -Wextra -Wno-missing-field-initializers -Wformat=2 \
    -Wswitch-default -Wswitch-enum -Wcast-align -Wpointer-arith \
    -Wbad-function-cast -Wstrict-overflow=5 -Winline \
    -Wundef -Wcast-qual -Wshadow -Wunreachable-code \
    -Wlogical-op -Wfloat-equal -Wredundant-decls \
    -Wold-style-definition \
    $(DEBUG) \
    -fno-omit-frame-pointer -ffloat-store -fno-common -fstrict-aliasing 

PKGCONFIG_LIBS := `pkg-config --cflags --libs libsystemd`

LDFLAGS := $(PKGCONFIG_LIBS) 

all: bin $(PROJECT)

$(PROJECT): $(OBJS)
	$(CC) $(CFLAGS) $^ ${LDFLAGS} -o $@

clean:
	-rm $(OBJS)
	-rm -r bin
	-rm plot-test 

bin/%.o %.c:
	$(CC) $(CFLAGS) $(INCLUDES_ARG) $*.c -c -o $@

# create a bin directory if it doesn't exist
bin:
	mkdir bin
