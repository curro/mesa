TOP = ../../../../..
include $(TOP)/configs/current

LIBNAME = ws_loader_sw

LIBRARY_INCLUDES = \
	-I$(TOP)/include \
	-I$(TOP)/src/gallium/include \
	-I$(TOP)/src/gallium/auxiliary \
	-I$(TOP)/src/gallium/winsys

LIBRARY_DEFINES = -DHAVE_WS_LOADER_SW

C_SOURCES = \
	ws_loader_sw.c

EXTRA_OBJECTS = \
	$(TOP)/src/gallium/winsys/sw/null/libws_null.a

ifneq ($(findstring sw/xlib,$(GALLIUM_WINSYS_DIRS)),)
LIBRARY_DEFINES += -DHAVE_WINSYS_XLIB
EXTRA_OBJECTS += $(TOP)/src/gallium/winsys/sw/null/libws_xlib.a
endif

include ../../../Makefile.template
