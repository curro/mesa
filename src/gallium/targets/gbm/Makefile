# src/gallium/targets/gbm/Makefile

TOP = ../../../..
include $(TOP)/configs/current

GBM_BACKEND = gbm_gallium_drm
GBM_SOURCES = gbm.c

GBM_INCLUDES = \
	       -I$(TOP)/include \
	       -I$(TOP)/src/gallium/state_trackers/gbm \
	       -I$(TOP)/src/gbm/main \
	       -I$(TOP)/src/gallium/auxiliary \
	       -I$(TOP)/src/gallium/winsys \
	       -I$(TOP)/src/gallium/include

GBM_LIBS = $(LIBUDEV_LIBS) $(LIBDRM_LIB) -lm \
	   $(TOP)/src/gallium/state_trackers/gbm/libgbm.a \
	   $(TOP)/src/gallium/winsys/loader/libws_loader.a \
	   $(TOP)/src/gallium/drivers/identity/libidentity.a \
	   $(TOP)/src/gallium/drivers/galahad/libgalahad.a \
	   $(TOP)/src/gallium/drivers/trace/libtrace.a \
	   $(TOP)/src/gallium/drivers/rbug/librbug.a \
	   $(GALLIUM_AUXILIARIES)

PIPE_INSTALL_DIR = $(GBM_BACKEND_INSTALL_DIR)
include $(TOP)/src/gallium/targets/pipe-loader/Makefile.template

GBM_CFLAGS = \
	     $(PIPE_DEFINES) \
	     $(LIBUDEV_CFLAGS) \
	     $(LIBDRM_CFLAGS)

GBM_EXTRA_TARGETS = $(addprefix $(TOP)/$(LIB_DIR)/gbm/, $(PIPE_TARGETS))
GBM_EXTRA_INSTALL = install-pipes
GBM_EXTRA_CLEAN = clean-pipes
GBM_EXTRA_SOURCES = $(PIPE_SOURCES)

include $(TOP)/src/gbm/backends/Makefile.template

$(GBM_EXTRA_TARGETS): $(TOP)/$(LIB_DIR)/gbm/%: %
	@$(INSTALL) -d $(dir $@)
	$(INSTALL) $< $(dir $@)
