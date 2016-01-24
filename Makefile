TARGET = libvdpau_rk3x.so.1
SRC = device.c presentation_queue.c  surface_video.c \
	mpeg2.c mpeg4.c h264.c decoder.c \
	surface_bitmap.c surface_output.c video_mixer.c  handles.c \
	 tiled_yuv.S rgb_asm.S

#CFLAGS ?= -Wall -Werror=implicit-function-declaration -O3 -I./include
CFLAGS ?= -Wall -Werror=implicit-function-declaration -g -O0 -I./include
LDFLAGS ?=
LIBS = -lrt -lm -lX11 -lpthread -ldecx170h -ldecx170m2 -ldecx170m -ldecx170p -ldecx170v -ldwlx170 -lx170j -lrklayers
CC ?= gcc

DEP_CFLAGS = -MD -MP -MQ $@
LIB_CFLAGS = -fpic -fvisibility=hidden
LIB_LDFLAGS = -shared -Wl,-soname,$(TARGET)

OBJ = $(addsuffix .o,$(basename $(SRC)))
DEP = $(addsuffix .d,$(basename $(SRC)))

MODULEDIR = $(shell pkg-config --variable=moduledir vdpau)

ifeq ($(MODULEDIR),)
MODULEDIR=/usr/lib/vdpau
endif

.PHONY: clean all install uninstall

all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) $(LIB_LDFLAGS) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

clean:
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -f $(TARGET)

install: $(TARGET)
	install -D $(TARGET) $(DESTDIR)$(MODULEDIR)/$(TARGET)
	ln -sf $(TARGET) $(DESTDIR)$(MODULEDIR)/$(basename $(TARGET))

uninstall:
	rm -f $(DESTDIR)$(MODULEDIR)/$(basename $(TARGET))
	rm -f $(DESTDIR)$(MODULEDIR)/$(TARGET)

%.o: %.c
	$(CC) $(DEP_CFLAGS) $(LIB_CFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) -c $< -o $@

include $(wildcard $(DEP))
