TOP = ../../../..
include $(TOP)/configs/current

LIBNAME = r600

LIBRARY_INCLUDES = \
	-I$(TOP)/include \
	-I$(TOP)/src/gallium/drivers/radeon/


# get C_SOURCES
include Makefile.sources

ifeq ($(HAVE_LLVM),0x0300)

LIBRADEON = $(TOP)/src/gallium/drivers/radeon/libradeon.a

EXTRA_OBJECTS = \
	$(LIBRADEON)

CLEAN_DEPS = \
	clean_radeon

DEFAULT_DEPS = \
	make_radeon

endif

include ../../Makefile.template

$(LIBRADEON): make_radeon

.PHONY: make_radeon

make_radeon:
	touch $(LIBRADEON)
	$(MAKE) -C $(TOP)/src/gallium/drivers/radeon/ default

clean_radeon:
	$(MAKE) -C $(TOP)/src/gallium/drivers/radeon/ clean
