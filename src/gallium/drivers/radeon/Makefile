
TOP = ../../../..
include $(TOP)/configs/current

include Makefile.sources

LIBNAME = radeon

LIBRARY_INCLUDES = -I$(TOP)/include

TBLGEN = $(LLVM_BINDIR)/llvm-tblgen

CXXFLAGS+= $(LLVM_CXXFLAGS)

include ../../Makefile.template

CXXFLAGS := $(filter-out -DDEBUG, $(CXXFLAGS))

tablegen = $(TBLGEN) -I $(LLVM_INCLUDEDIR) $1 $2 -o $3

gen: $(GENERATED_SOURCES)

R600ShaderPatterns.td: AMDGPUGenShaderPatterns.pl
	$(PERL) $^ C > $@
	
R600RegisterInfo.td: R600GenRegisterInfo.pl
	$(PERL) $^ > $@

AMDGPUInstrEnums.td: AMDGPUGenInstrEnums.pl
	$(PERL) $^ td > $@

AMDGPUInstrEnums.h.inc: AMDGPUGenInstrEnums.pl
	$(PERL) $^ h > $@

AMDGPUInstrEnums.inc: AMDGPUGenInstrEnums.pl
	$(PERL) $^ inc > $@


AMDILGenRegisterInfo.inc: *.td
	$(call tablegen, -gen-register-info, AMDGPU.td, $@)

AMDILGenInstrInfo.inc: *.td
	$(call tablegen, -gen-instr-info, AMDGPU.td, $@)

AMDILGenAsmWriter.inc: *.td
	$(call tablegen, -gen-asm-writer, AMDGPU.td, $@)

AMDILGenDAGISel.inc: *.td
	$(call tablegen, -gen-dag-isel, AMDGPU.td, $@)

AMDILGenCallingConv.inc: *.td
	$(call tablegen, -gen-callingconv, AMDGPU.td, $@)

AMDILGenSubtarget.inc: *.td
	$(call tablegen, -gen-subtarget, AMDGPU.td, $@)

AMDILGenEDInfo.inc: *.td
	$(call tablegen, -gen-enhanced-disassembly-info, AMDGPU.td, $@)

AMDILGenIntrinsics.inc: *.td
	$(call tablegen, -gen-tgt-intrinsic, AMDGPU.td, $@)
	#Tablgen uses the Target of the first intrinsic for all intrinsics,
	#and AMDILIntrinsicInfo.cpp expecects the Target to be AMDILIntrinsic,
	#so we need to change it here.
	sed -i 's/AMDGPUIntrinsic/AMDILIntrinsic/g' $@

AMDGPUGenCodeEmitter.inc: *.td
	$(call tablegen, -gen-emitter, AMDGPU.td, $@)
	sed -i 's/unsigned/uint64_t/g' $@

%.td: ;

LOADER_LIBS=$(shell llvm-config --libs bitreader asmparser)

loader: loader.o libradeon.a
	gcc -o loader -L/usr/local/lib $(LDFLAGS) loader.o libradeon.a $(LLVM_LIBS) $(LOADER_LIBS) -lpthread -ldl -lstdc++ -lm
