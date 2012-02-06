
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

R600ShaderPatterns.td: AMDISAGenShaderPatterns.pl
	$(PERL) $^ C > $@
	
R600RegisterInfo.td: R600GenRegisterInfo.pl
	$(PERL) $^ > $@

AMDISAInstrEnums.td: AMDISAGenInstrEnums.pl
	$(PERL) $^ td > $@

AMDISAInstrEnums.h.inc: AMDISAGenInstrEnums.pl
	$(PERL) $^ h > $@

AMDISAInstrEnums.inc: AMDISAGenInstrEnums.pl
	$(PERL) $^ inc > $@


AMDILGenRegisterInfo.inc: *.td
	$(call tablegen, -gen-register-info, AMDISA.td, $@)

AMDILGenInstrInfo.inc: *.td
	$(call tablegen, -gen-instr-info, AMDISA.td, $@)

AMDILGenAsmWriter.inc: *.td
	$(call tablegen, -gen-asm-writer, AMDISA.td, $@)

AMDILGenDAGISel.inc: *.td
	$(call tablegen, -gen-dag-isel, AMDISA.td, $@)

AMDILGenCallingConv.inc: *.td
	$(call tablegen, -gen-callingconv, AMDISA.td, $@)

AMDILGenSubtarget.inc: *.td
	$(call tablegen, -gen-subtarget, AMDISA.td, $@)

AMDILGenEDInfo.inc: *.td
	$(call tablegen, -gen-enhanced-disassembly-info, AMDISA.td, $@)

AMDILGenIntrinsics.inc: *.td
	$(call tablegen, -gen-tgt-intrinsic, AMDISA.td, $@)

AMDISAGenCodeEmitter.inc: *.td
	$(call tablegen, -gen-emitter, AMDISA.td, $@)

%.td: ;

LOADER_LIBS=$(shell llvm-config --libs)

loader: loader.o libradeon.a
	gcc -o loader -L/usr/local/lib $(LDFLAGS) loader.o libradeon.a $(LOADER_LIBS) -lpthread -ldl -lstdc++ -lm
