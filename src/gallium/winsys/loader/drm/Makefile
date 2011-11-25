TOP = ../../../../..
include $(TOP)/configs/current

LIBNAME = ws_loader_drm

LIBRARY_INCLUDES = \
	-I$(TOP)/include \
	-I$(TOP)/src/gallium/include \
	-I$(TOP)/src/gallium/auxiliary \
	-I$(TOP)/src/gallium/winsys \
	$(LIBDRM_CFLAGS)

LIBRARY_DEFINES = -DHAVE_WS_LOADER_DRM

C_SOURCES = \
	ws_loader_drm.c

include ../../../Makefile.template
