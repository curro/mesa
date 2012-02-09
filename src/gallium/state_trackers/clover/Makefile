TOP = ../../../..
include $(TOP)/configs/current

LIBNAME = OpenCL
OPENCL_MAJOR = 1
OPENCL_MINOR = 0

INCLUDES = -I$(TOP)/src/gallium/include \
	   -I$(TOP)/src/gallium/drivers \
	   -I$(TOP)/src/gallium/auxiliary \
	   -I$(TOP)/src/gallium/winsys \
	   -I.

DEFINES = -DMESA_VERSION=\"$(MESA_VERSION)\" \
	  $(PIPE_DEFINES)

LIBS =	$(TOP)/src/gallium/winsys/loader/libws_loader.a \
	$(TOP)/src/gallium/winsys/loader/drm/libws_loader_drm.a \
	$(GALLIUM_AUXILIARIES) \
	$(LIBUDEV_LIBS) \
	-ldl

CPP_SOURCES = \
	core/device.cpp \
	core/context.cpp \
	core/format.cpp \
	core/memory.cpp \
	core/sampler.cpp \
	core/event.cpp \
	core/program.cpp \
	core/kernel.cpp \
	api/platform.cpp \
	api/device.cpp \
	api/context.cpp \
	api/queue.cpp \
	api/memory.cpp \
	api/transfer.cpp \
	api/sampler.cpp \
	api/event.cpp \
	api/program.cpp \
	api/kernel.cpp


OBJECTS = $(C_SOURCES:.c=.o) \
	$(CPP_SOURCES:.cpp=.o) \
	$(ASM_SOURCES:.S=.o)
TARGET = $(TOP)/$(LIB_DIR)/lib$(LIBNAME).so
CXXFLAGS += -std=c++0x

PIPE_INSTALL_DIR = $(OPENCL_LIB_INSTALL_DIR)

##### RULES #####

%.o: %.c
	$(CC) -c $(INCLUDES) $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) -c $(INCLUDES) $(CXXFLAGS) $< -o $@

%.o: %.S
	$(CC) -c $(INCLUDES) $(CFLAGS)  $< -o $@

##### TARGETS #####

default: depend $(TARGET)

include $(TOP)/src/gallium/targets/pipe-loader/Makefile.template

$(TARGET): $(OBJECTS) $(LIBS) $(PIPE_TARGETS) Makefile
	$(MKLIB) -o $(LIBNAME) -linker '$(CXX)' -ldflags '$(LDFLAGS)' \
		-major $(OPENCL_MAJOR) -minor $(OPENCL_MINOR) $(MKLIB_OPTIONS) \
		-install $(TOP)/$(LIB_DIR) $(OBJECTS) $(LIBS)

depend: $(C_SOURCES) $(CPP_SOURCES) $(ASM_SOURCES) $(SYMLINKS)
	rm -f depend
	touch depend
	$(MKDEP) $(MKDEP_OPTIONS) $(DEFINES) $(INCLUDES) $(C_SOURCES) \
		 $(CPP_SOURCES) $(ASM_SOURCES) 2> /dev/null

# Remove .o and backup files
clean: clean-pipes
	-rm -f $(OBJECTS) *~ *.so $(SYMLINKS)
	-rm -f depend depend.bak

install: default install-pipes
	$(MINSTALL) -m 755 $(TOP)/$(LIB_DIR)/lib$(LIBNAME).so* $(INSTALL_LIB_DIR)

sinclude depend
