TOP = ../../../..
include $(TOP)/configs/current

LIBNAME = ws_loader

LIBRARY_INCLUDES = \
	-I$(TOP)/include \
	-I$(TOP)/src/gallium/include \
	-I$(TOP)/src/gallium/auxiliary \
	-I$(TOP)/src/gallium/winsys

C_SOURCES = \
	ws_loader.c

ifneq ($(findstring loader/sw,$(GALLIUM_WINSYS_DIRS)),)
EXTRA_OBJECTS += sw/libws_loader_sw.a
LIBRARY_DEFINES += -DHAVE_WS_LOADER_SW
endif

ifneq ($(findstring loader/drm,$(GALLIUM_WINSYS_DIRS)),)
EXTRA_OBJECTS += drm/libws_loader_drm.a
LIBRARY_DEFINES += -DHAVE_WS_LOADER_DRM
endif

$(EXTRA_OBJECTS):
	@(cd $(dir $@) && $(MAKE) $(notdir $@)) || exit 1

include ../../Makefile.template
