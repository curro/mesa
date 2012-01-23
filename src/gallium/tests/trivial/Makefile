# src/gallium/tests/trivial/Makefile

TOP = ../../../..
include $(TOP)/configs/current

INCLUDES = \
	-I. \
	-I$(TOP)/src/gallium/include \
	-I$(TOP)/src/gallium/auxiliary \
	-I$(TOP)/src/gallium/drivers \
	-I$(TOP)/src/gallium/winsys \
	$(PROG_INCLUDES)

LINKS += \
	$(TOP)/src/gallium/drivers/rbug/librbug.a \
	$(TOP)/src/gallium/drivers/trace/libtrace.a \
	$(TOP)/src/gallium/drivers/galahad/libgalahad.a \
	$(TOP)/src/gallium/winsys/loader/libws_loader.a \
	$(GALLIUM_AUXILIARIES) \
	$(PROG_LINKS) $(LIBUDEV_LIBS)

LDFLAGS += $(LLVM_LDFLAGS)

SOURCES = \
	tri.c \
	quad-tex.c \
	compute.c

OBJECTS = $(SOURCES:.c=.o)

PROGS = $(OBJECTS:.o=)

PROG_DEFINES = \
	-DGALLIUM_SOFTPIPE -DGALLIUM_RBUG -DGALLIUM_TRACE -DGALLIUM_GALAHAD

##### TARGETS #####

default: $(PROGS)

include $(TOP)/src/gallium/targets/pipe-loader/Makefile.template

clean:
	-rm -f $(PROGS)
	-rm -f *.o
	-rm -f result.bmp

##### RULES #####

$(OBJECTS): %.o: %.c
	$(CC) -c $(INCLUDES) $(CFLAGS) $(DEFINES) $(PROG_DEFINES) $(PIPE_DEFINES) $< -o $@

$(PROGS): %: %.o $(LINKS) $(PIPE_TARGETS)
	$(CXX) $(LDFLAGS) $< $(LINKS) $(LLVM_LIBS) -lm -lpthread -ldl -o $@
