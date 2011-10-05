// Copyright (c) 2011, Advanced Micro Devices, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// Neither the name of the copyright holder nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// If you use the software (in whole or in part), you shall adhere to all
// applicable U.S., European, and other export laws, including but not limited
// to the U.S. Export Administration Regulations (“EAR”), (15 C.F.R. Sections
// 730 through 774), and E.U. Council Regulation (EC) No 1334/2000 of 22 June
// 2000.  Further, pursuant to Section 740.6 of the EAR, you hereby certify
// that, except pursuant to a license granted by the United States Department
// of Commerce Bureau of Industry and Security or as otherwise permitted
// pursuant to a License Exception under the U.S. Export Administration
// Regulations ("EAR"), you will not (1) export, re-export or release to a
// national of a country in Country Groups D:1, E:1 or E:2 any restricted
// technology, software, or source code you receive hereunder, or (2) export to
// Country Groups D:1, E:1 or E:2 the direct product of such technology or
// software, if such foreign produced direct product is subject to national
// security controls as identified on the Commerce Control List (currently
// found in Supplement 1 to Part 774 of EAR).  For the most current Country
// Group listings, or for additional information about the EAR or your
// obligations under those regulations, please refer to the U.S. Bureau of
// Industry and Security’s website at http://www.bis.doc.gov/.
//
//==-----------------------------------------------------------------------===//
#define DEBUG_TYPE "asm-printer"
#if !defined(NDEBUG)
# define DEBUGME (DebugFlag && isCurrentDebugType(DEBUG_TYPE))
#else
# define DEBUGME (false)
#endif
#include "AMDILAsmPrinter.h"
#include "AMDILAlgorithms.tpp"
#include "AMDILCompilerErrors.h"
#include "AMDILDevices.h"
#include "AMDILGlobalManager.h"
#include "AMDILKernelManager.h"
#include "AMDILMachineFunctionInfo.h"
#include "AMDILUtilityFunctions.h"
#include "llvm/Constants.h"
#include "llvm/Metadata.h"
#include "llvm/Type.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DebugLoc.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/TargetRegistry.h"
#include <sstream>
using namespace llvm;
/// createAMDILCodePrinterPass - Returns a pass that prints the AMDIL
/// assembly code for a MachineFunction to the given output stream,
/// using the given target machine description. This should work
/// regardless of whether the function is in SSA form.
///

  ASMPRINTER_RETURN_TYPE
createAMDILCodePrinterPass(AMDIL_ASM_PRINTER_ARGUMENTS)
{
  const AMDILSubtarget *stm = &TM.getSubtarget<AMDILSubtarget>();
  return stm->device()->getAsmPrinter(ASM_PRINTER_ARGUMENTS);
}

#include "AMDILGenAsmWriter.inc"

/*
// Force static initialization
extern "C" void LLVMInitializeAMDILAsmPrinter() {
  llvm::TargetRegistry::RegisterAsmPrinter(TheAMDILTarget,
      createAMDILCodePrinterPass);
}
*/
AMDILInstPrinter *llvm::createAMDILInstPrinter(const MCAsmInfo &MAI) {
  return new AMDILInstPrinter(MAI);
}

//
// @param name
// @brief strips KERNEL_PREFIX and KERNEL_SUFFIX from the name
// and returns that name if both of the tokens are present.
//
  static
std::string Strip(const std::string &name)
{
  size_t start = name.find("__OpenCL_");
  size_t end = name.find("_kernel");
  if (start == std::string::npos
      || end == std::string::npos
      || (start == end)) {
    return name;
  } else {
    return name.substr(9, name.length()-16);
  }
}
// TODO: Add support for verbose.
  AMDILAsmPrinter::AMDILAsmPrinter(AMDIL_ASM_PRINTER_ARGUMENTS)
: AsmPrinter(ASM_PRINTER_ARGUMENTS)
{
  mDebugMode = DEBUGME;
  mTM = reinterpret_cast<AMDILTargetMachine*>(&TM);
  mTM->setDebug(mDebugMode);
  mGlobal = new AMDILGlobalManager(mDebugMode);
  mMeta = new AMDILKernelManager(mTM, mGlobal);
  mBuffer = 0;
  mNeedVersion = false;
}

AMDILAsmPrinter::~AMDILAsmPrinter()
{
  delete mMeta;
  delete mGlobal;
}
const char*
AMDILAsmPrinter::getPassName() const
{
  return "AMDIL Assembly Printer";
}

void
AMDILAsmPrinter::EmitInstruction(const MachineInstr *II)
{
  std::string FunStr;
  raw_string_ostream OFunStr(FunStr);
  formatted_raw_ostream O(OFunStr);
  const AMDILSubtarget *curTarget = mTM->getSubtargetImpl();
  if (mDebugMode) {
    O << ";" ;
    II->print(O);
  }
  if (isMacroFunc(II)) {
    emitMacroFunc(II, O);
    O.flush();
    OutStreamer.EmitRawText(StringRef(FunStr));
    return;
  }
  if (isMacroCall(II)) {
    const char *name = II->getDesc().getName() + 5;
    int macronum = amd::MacroDBFindMacro(name);
    O << "\t;"<< name<<"\n";
    O << "\tmcall("<<macronum<<")";
    if (curTarget->device()->isSupported(
          AMDILDeviceInfo::MacroDB)) {
      mMacroIDs.insert(macronum);
    } else {
      mMFI->addCalledIntr(macronum);
    }
  }

  printInstruction(II, O);
  O.flush();
  OutStreamer.EmitRawText(StringRef(FunStr));
}
  void
AMDILAsmPrinter::emitMacroFunc(const MachineInstr *MI,
    OSTREAM_TYPE &O)
{
  const char *name = "unknown";
  llvm::StringRef nameRef;
  if (MI->getOperand(0).isGlobal()) {
    nameRef = MI->getOperand(0).getGlobal()->getName();
    name = nameRef.data();
  }
  emitMCallInst(MI, O, name);
}

  bool
AMDILAsmPrinter::runOnMachineFunction(MachineFunction &lMF)
{
  this->MF = &lMF;
  mMeta->setMF(&lMF);
  mMFI = lMF.getInfo<AMDILMachineFunctionInfo>();
  SetupMachineFunction(lMF);
  std::string kernelName = CurrentFnSym->getName();
  mName = Strip(kernelName);

  mKernelName = kernelName;
  EmitFunctionHeader();
  EmitFunctionBody();
  return false;
}
  void
AMDILAsmPrinter::addCPoolLiteral(const Constant *C)
{
  if (const ConstantFP *CFP = dyn_cast<ConstantFP>(C)) {
    if (CFP->getType()->isFloatTy()) {
      mMFI->addf32Literal(CFP);
    } else {
      mMFI->addf64Literal(CFP);
    }
  } else if (const ConstantInt *CI = dyn_cast<ConstantInt>(C)) {
    int64_t val = 0;
    if (CI) {
      val = CI->getSExtValue();
    }
    if (CI->getBitWidth() == (int64_t)64) {
      mMFI->addi64Literal(val);
    } else if (CI->getBitWidth() == (int64_t)8) {
      mMFI->addi32Literal((uint32_t)val, AMDIL::LOADCONST_i8);
    } else if (CI->getBitWidth() == (int64_t)16) {
      mMFI->addi32Literal((uint32_t)val, AMDIL::LOADCONST_i16);
    } else {
      mMFI->addi32Literal((uint32_t)val, AMDIL::LOADCONST_i32);
    }
  } else if (const ConstantArray *CA = dyn_cast<ConstantArray>(C)) {
    uint32_t size = CA->getNumOperands();
    for (uint32_t x = 0; x < size; ++x) {
      addCPoolLiteral(CA->getOperand(x));
    }
  } else if (const ConstantAggregateZero *CAZ
      = dyn_cast<ConstantAggregateZero>(C)) {
    if (CAZ->isNullValue()) {
      mMFI->addi32Literal(0, AMDIL::LOADCONST_i32);
      mMFI->addi64Literal(0);
      mMFI->addf64Literal(0);
      mMFI->addf32Literal(0);
    }
  } else if (const ConstantStruct *CS = dyn_cast<ConstantStruct>(C)) {
    uint32_t size = CS->getNumOperands();
    for (uint32_t x = 0; x < size; ++x) {
      addCPoolLiteral(CS->getOperand(x));
    }
#if LLVM_VERSION < 2500
  } else if (const ConstantUnion *CU = dyn_cast<ConstantUnion>(C)) {
    uint32_t size = CU->getNumOperands();
    for (uint32_t x = 0; x < size; ++x) {
      addCPoolLiteral(CU->getOperand(x));
    }
#endif
  } else if (const ConstantVector *CV = dyn_cast<ConstantVector>(C)) {
    // TODO: Make this handle vectors natively up to the correct
    // size
    uint32_t size = CV->getNumOperands();
    for (uint32_t x = 0; x < size; ++x) {
      addCPoolLiteral(CV->getOperand(x));
    }
  } else {
    // TODO: Do we really need to handle ConstantPointerNull?
    // What about BlockAddress, ConstantExpr and Undef?
    // How would these even be generated by a valid CL program?
    assert(0 && "Found a constant type that I don't know how to handle");
  }
}

  void
AMDILAsmPrinter::EmitGlobalVariable(const GlobalVariable *GV)
{
  llvm::StringRef GVname = GV->getName();
  SmallString<1024> Str;
  raw_svector_ostream O(Str);
  const AMDILSubtarget *curTarget = mTM->getSubtargetImpl();
  int32_t autoSize = curTarget->getGlobalManager()->getArrayOffset(GVname);
  int32_t constSize = curTarget->getGlobalManager()->getConstOffset(GVname);
  O << ".global@" << GVname;
  if (autoSize != -1) {
    O << ":" << autoSize << "\n";
  } else if (constSize != -1) {
    O << ":" << constSize << "\n";
  }
  O.flush();
  OutStreamer.EmitRawText(O.str());
}

void
AMDILAsmPrinter::printOperand(const MachineInstr *MI, int opNum
#if LLVM_VERSION >= 2351
    , OSTREAM_TYPE &O
#endif
    )
{
  const MachineOperand &MO = MI->getOperand (opNum);

  switch (MO.getType()) {
    case MachineOperand::MO_Register:
      if (MO.isReg()) {
        if ((signed)MO.getReg() < 0) {
          // FIXME: we need to remove all virtual register creation after register allocation.
          // This is a work-around to make sure that the virtual register range does not 
          // clobber the physical register range.
          O << "r" << ((MO.getReg() & 0x7FFFFFFF)  + 2048) << getSwizzle(MI, opNum);
        } else {
          O << getRegisterName(MO.getReg()) << getSwizzle(MI, opNum);
        }
      } else {
        assert(0 && "Invalid Register type");
        mMFI->addErrorMsg(amd::CompilerErrorMessage[INTERNAL_ERROR]);
      }
      break;
    case MachineOperand::MO_Immediate:
    case MachineOperand::MO_FPImmediate:
      {
        unsigned opcode = MI->getOpcode();
        if ((opNum == (int)(MI->getNumOperands() - 1))
             && (opcode >= AMDIL::ATOM_A_ADD
            && opcode <= AMDIL::ATOM_R_XOR_NORET
            || (opcode >= AMDIL::SCRATCHLOAD 
              && opcode <= AMDIL::SCRATCHSTORE_ZW)
            || (opcode >= AMDIL::LDSLOAD && opcode <= AMDIL::LDSSTORE_i8)
            || (opcode >= AMDIL::GDSLOAD && opcode <= AMDIL::GDSSTORE_Z)
            || (opcode >= AMDIL::UAVARENALOAD_W_i32
              && opcode <= AMDIL::UAVRAWSTORE_v4i32)
            || opcode == AMDIL::CBLOAD
            || opcode == AMDIL::CASE)
           ){
          O << MO.getImm();
        } else if (((opcode >= AMDIL::VEXTRACT_v2f32
                && opcode <= AMDIL::VEXTRACT_v4i8)
              && (opNum == 2))) {
          // The swizzle is encoded in the operand so the
          // literal that represents the swizzle out of ISel
          // can be ignored.
        } else if ((opcode >= AMDIL::VINSERT_v2f32)
            && (opcode <= AMDIL::VINSERT_v4i8)
            && ((opNum == 3)  || (opNum == 4))) {
          // The swizzle is encoded in the operand so the
          // literal that represents the swizzle out of ISel
          // can be ignored.
          // The swizzle is encoded in the operand so the
          // literal that represents the swizzle out of ISel
          // can be ignored.
        } else if (opNum == 1 && 
            (opcode == AMDIL::APPEND_ALLOC
           || opcode == AMDIL::APPEND_ALLOC_NORET
           || opcode == AMDIL::APPEND_CONSUME
           || opcode == AMDIL::APPEND_CONSUME_NORET
           || opcode == AMDIL::IMAGE2D_READ
           || opcode == AMDIL::IMAGE3D_READ
           || opcode == AMDIL::IMAGE2D_READ_UNNORM
           || opcode == AMDIL::IMAGE3D_READ_UNNORM
           || opcode == AMDIL::CBLOAD)) {
          // We don't need to emit the 'l' so we just emit
          // the immediate as it stores the resource ID and
          // is not a true literal.
          O << MO.getImm();
        } else if (opNum == 0 && 
            (opcode == AMDIL::IMAGE2D_READ
             || opcode == AMDIL::IMAGE3D_READ
             || opcode == AMDIL::IMAGE2D_READ_UNNORM
             || opcode == AMDIL::IMAGE3D_READ_UNNORM
             || opcode == AMDIL::IMAGE2D_WRITE
             || opcode == AMDIL::IMAGE3D_WRITE)) {
          O << MO.getImm();
        } else if (opNum == 3 &&
            (opcode == AMDIL::IMAGE2D_READ
             || opcode == AMDIL::IMAGE2D_READ_UNNORM
             || opcode == AMDIL::IMAGE3D_READ_UNNORM
             || opcode == AMDIL::IMAGE3D_READ)) {
          O << MO.getImm();
        } else if (MO.isImm() || MO.isFPImm()) {
          O << "l" << MO.getImm() << getSwizzle(MI, opNum);
        } else {
          assert(0 && "Invalid literal/constant type");
          mMFI->addErrorMsg(
              amd::CompilerErrorMessage[INTERNAL_ERROR]);
        }
      }
      break;
    case MachineOperand::MO_MachineBasicBlock:
      EmitBasicBlockStart(MO.getMBB());
      return;
    case MachineOperand::MO_GlobalAddress:
      {
        int offset = 0;
        const GlobalValue *gv = MO.getGlobal();
        // Here we look up by the name for the corresponding number
        // and we print that out instead of the name or the address
        if (MI->getOpcode() == AMDIL::CALL) {
          uint32_t funcNum;
          llvm::StringRef name = gv->getName();
          funcNum = name.empty() 
            ?  mGlobal->getOrCreateFunctionID(gv)
            : mGlobal->getOrCreateFunctionID(name);
          mMFI->addCalledFunc(funcNum);
          O << funcNum <<" ; "<< name;
        } else if((offset = mGlobal->getArrayOffset(gv->getName()))
            != -1) {
          mMFI->setUsesLocal();
          O << "l" << mMFI->getIntLits(offset) << ".x";
        } else if((offset = mGlobal->getConstOffset(gv->getName()))
            != -1) {
          mMFI->addMetadata(";memory:datareqd");
          O << "l" << mMFI->getIntLits(offset) << ".x";
        } else {
          assert(0 && "GlobalAddress without a function call!");
          mMFI->addErrorMsg(
              amd::CompilerErrorMessage[MISSING_FUNCTION_CALL]);
        }
      }
      break;
    case MachineOperand::MO_ExternalSymbol:
      {
        if (MI->getOpcode() == AMDIL::CALL) {
          uint32_t funcNum = mGlobal->getOrCreateFunctionID(
              std::string(MO.getSymbolName()));
          mMFI->addCalledFunc(funcNum);
          O << funcNum << " ; "<< MO.getSymbolName();
          // This is where pointers should get resolved
        } else {
          assert(0 && "ExternalSymbol without a function call!");
          mMFI->addErrorMsg(
              amd::CompilerErrorMessage[MISSING_FUNCTION_CALL]);
        }
      }
      break;
    case MachineOperand::MO_ConstantPoolIndex:
      {
        // Copies of constant buffers need to be done here
        const kernel &tmp = mGlobal->getKernel(mKernelName);
        O << "l" << mMFI->getIntLits(
            tmp.CPOffsets[MO.getIndex()].first);
      }
      break;
    default:
      O << "<unknown operand type>"; break;
  }
}

void
AMDILAsmPrinter::printMemOperand(
    const MachineInstr *MI,
    int opNum,
#if LLVM_VERSION >= 2351
    OSTREAM_TYPE &O,
#endif
    const char *Modifier
    )
{
  const MachineOperand &MO = MI->getOperand (opNum);
  if (opNum != 1) {
    printOperand(MI, opNum
#if LLVM_VERSION >= 2351
        , O
#endif
        );
  } else {
    switch (MO.getType()) {
      case MachineOperand::MO_Register:
        if (MO.isReg()) {
          if ((signed)MO.getReg() < 0) {
            // FIXME: we need to remove all virtual register creation after register allocation.
            // This is a work-around to make sure that the virtual register range does not 
            // clobber the physical register range.
            O << "r" << ((MO.getReg() & 0x7FFFFFFF) + 2048) << getSwizzle(MI, opNum);
          } else {
            O << getRegisterName(MO.getReg()) << getSwizzle(MI, opNum);
          }
        }
        else {
          assert(0 && "Invalid Register type");
          mMFI->addErrorMsg(
              amd::CompilerErrorMessage[INTERNAL_ERROR]);
        }
        break;
      case MachineOperand::MO_Immediate:
      case MachineOperand::MO_FPImmediate:
        {
          unsigned opcode = MI->getOpcode();
        if ((opNum == (int)(MI->getNumOperands() - 1))
            && (opcode >= AMDIL::ATOM_A_ADD
            && opcode <= AMDIL::ATOM_R_XOR_NORET
            || (opcode >= AMDIL::SCRATCHLOAD 
              && opcode <= AMDIL::SCRATCHSTORE_ZW)
            || (opcode >= AMDIL::LDSLOAD && opcode <= AMDIL::LDSSTORE_i8)
            || (opcode >= AMDIL::GDSLOAD && opcode <= AMDIL::GDSSTORE_Z)
            || (opcode >= AMDIL::UAVARENALOAD_W_i32
              && opcode <= AMDIL::UAVRAWSTORE_v4i32)
            || opcode == AMDIL::CBLOAD
            || opcode == AMDIL::CASE)
            ) {
          O << MO.getImm();
        } else if (opNum == 1 && 
            (opcode == AMDIL::APPEND_ALLOC
             || opcode == AMDIL::APPEND_ALLOC_NORET
             || opcode == AMDIL::APPEND_CONSUME
             || opcode == AMDIL::APPEND_CONSUME_NORET
             || opcode == AMDIL::IMAGE2D_READ
             || opcode == AMDIL::IMAGE3D_READ
             || opcode == AMDIL::IMAGE2D_READ_UNNORM
             || opcode == AMDIL::IMAGE3D_READ_UNNORM
             || opcode == AMDIL::CBLOAD)) {
          // We don't need to emit the 'l' so we just emit
          // the immediate as it stores the resource ID and
          // is not a true literal.
          O << MO.getImm();
        } else if (opNum == 0 && 
            (opcode == AMDIL::IMAGE2D_READ
             || opcode == AMDIL::IMAGE3D_READ
             || opcode == AMDIL::IMAGE2D_READ_UNNORM
             || opcode == AMDIL::IMAGE3D_READ_UNNORM
             || opcode == AMDIL::IMAGE2D_WRITE
             || opcode == AMDIL::IMAGE3D_WRITE)) {
          O << MO.getImm();
        } else if (opNum == 3 &&
            (opcode == AMDIL::IMAGE2D_READ
             || opcode == AMDIL::IMAGE2D_READ_UNNORM
             || opcode == AMDIL::IMAGE3D_READ_UNNORM
             || opcode == AMDIL::IMAGE3D_READ)) {
          O << MO.getImm();
        } else if (MO.isImm() || MO.isFPImm()) {
          O << "l" << MO.getImm();
        }
        else {
          assert(0 && "Invalid literal/constant type");
          mMFI->addErrorMsg(
              amd::CompilerErrorMessage[INTERNAL_ERROR]);
        }
        }
        break;
      case MachineOperand::MO_ConstantPoolIndex:
        {
          // Copies of constant buffers need to be done here
          const kernel &tmp = mGlobal->getKernel(mKernelName);
          O << "l" << mMFI->getIntLits(
              tmp.CPOffsets[MO.getIndex()].first);
        }
        break;
      default:
        O << "<unknown operand type>"; break;
    };
  }
}

  const char*
AMDILAsmPrinter::getSwizzle(const MachineInstr *MI, int opNum)
{
  const MachineOperand &MO = MI->getOperand(opNum);
  OpSwizzle swiz;
  swiz.u8all = MO.getTargetFlags();
  if (!swiz.bits.dst) {
    return getSrcSwizzle(swiz.bits.swizzle);
  } else {
    return getDstSwizzle(swiz.bits.swizzle);
  }
}

  void
AMDILAsmPrinter::EmitStartOfAsmFile(Module &M)
{
#if LLVM_VERSION >= 2351
  SmallString<1024> Str;
  raw_svector_ostream O(Str);
#endif
  const AMDILSubtarget *curTarget = mTM->getSubtargetImpl();
  curTarget->setGlobalManager(mGlobal);
  curTarget->setKernelManager(mMeta);
  mGlobal->processModule(M, mTM);
  for (Module::const_iterator I = M.begin(), E = M.end(); I != E; ++I) {
    // Map all the known names to a unique number
    mGlobal->getOrCreateFunctionID(I);
  }

  if (curTarget->device()->isSupported(
        AMDILDeviceInfo::MacroDB)) {
    // Since we are using the macro db, the first token must be a macro.
    // So we make up a macro that is never used.
    // I originally picked -1, but the IL text translater treats them as
    // unsigned integers.
    O << "mdef(16383)_out(1)_in(2)\n";
    O << "mov r0, in0\n";
    O << "mov r1, in1\n";
    O << "div_zeroop(infinity) r0.x___, r0.x, r1.x\n";
    O << "mov out0, r0\n";
    O << "mend\n";
  }


  // We need to increase the number of reserved literals for
  // any literals we output manually instead of via the
  // emitLiteral function. This function should never
  // have any executable code in it. Only declarations
  // and the main function patch symbol.
  if (curTarget->device()->getGeneration() == AMDILDeviceInfo::HDTEST) {
    O << "il_cs_3_0\n";
  } else {
    O << "il_cs_2_0\n";
  }
  O << "dcl_cb cb0[10] ; Constant buffer that holds ABI data\n";
  O << "dcl_literal l0, 4, 1, 2, 3\n";
  O << "dcl_literal l1, 0x00FFFFFF, -1, -2, -3\n";
  O << "dcl_literal l2, 0x0000FFFF, 0xFFFFFFFE,0x000000FF,0xFFFFFFFC\n";
  O << "dcl_literal l3, 24, 16, 8, 0xFFFFFFFF\n";
  O << "dcl_literal l4, 0xFFFFFF00, 0xFFFF0000, 0xFF00FFFF, 0xFFFF00FF\n";
  O << "dcl_literal l5, 0, 4, 8, 12\n";
  O << "dcl_literal l6, 32, 32, 32, 32\n";
  O << "dcl_literal l7, 24, 31, 16, 31\n";
  O << ";$$$$$$$$$$\n";
  O << "endmain\n";
  O << ";DEBUGSTART\n";
#if LLVM_VERSION >= 2351
  OutStreamer.EmitRawText(O.str());
#endif
}
  void
AMDILAsmPrinter::EmitEndOfAsmFile(Module &M)
{
#if LLVM_VERSION >= 2351
  SmallString<1024> Str;
  raw_svector_ostream O(Str);
#endif
  const AMDILSubtarget *curTarget = mTM->getSubtargetImpl();
  O << ";DEBUGEND\n";
   if (curTarget->device()->isSupported(AMDILDeviceInfo::MacroDB)) {
    int lines;
    for (llvm::DenseSet<uint32_t>::iterator msb = mMacroIDs.begin()
        , mse = mMacroIDs.end(); msb != mse; ++msb) {
      int idx = *msb;
      const char* *macro = amd::MacroDBGetMacro(&lines, idx);
      for (int k = 0; k < lines; ++k) {
        O << macro[k];
      }
    }
  }
  mGlobal->dumpDataSection(O, mMeta);
  O << "\nend\n";
#ifdef _DEBUG
  if (mDebugMode) {
    mGlobal->print(O);
    mTM->dump(O);
  }
#endif
#if LLVM_VERSION >= 2351
  OutStreamer.EmitRawText(O.str());
#endif
}
void
AMDILAsmPrinter::PrintSpecial(const MachineInstr *MI, const char *Code) const
{
  assert(0 && "When is this function hit!");
}
  bool
AMDILAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned int OpNo,
    unsigned int AsmVariant, const char *ExtraCode)
{
  assert(0 && "When is this function hit!");
  return false;
}
  bool
AMDILAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
    unsigned int OpNo, unsigned int AsmVariant, const char *ExtraCode)
{
  assert(0 && "When is this function hit!");
  return false;
}
  void
AMDILAsmPrinter::EmitMachineConstantPoolValue(MachineConstantPoolValue *MCPV)
{
  assert(0 && "When is this function hit!");
}
void
AMDILAsmPrinter::printPICJumpTableSetLabel(unsigned uid,
    const MachineBasicBlock *MBB) const
{
  assert(0 && "When is this function hit!");
}
void
AMDILAsmPrinter::printPICJumpTableSetLabel(unsigned uid, unsigned uid2,
    const MachineBasicBlock *MBB) const
{
  assert(0 && "When is this function hit!");
}
void
AMDILAsmPrinter::printPICJumpTableEntry(const MachineJumpTableInfo *MJTI,
    const MachineBasicBlock *MBB,
    unsigned uid) const
{
  assert(0 && "When is this function hit!");
}

  void
AMDILAsmPrinter::EmitFunctionBodyStart()
{
  SmallString<1024> Str;
  raw_svector_ostream O(Str);

  bool isKernel = false;
  O << "";
  O << ";DEBUGEND\n";
  ++mBuffer;
  isKernel = mGlobal->isKernel(mKernelName);
  uint32_t id = mName.empty() 
    ? mGlobal->getOrCreateFunctionID(MF->getFunction())
        : mGlobal->getOrCreateFunctionID(mName);
  mMeta->setKernel(isKernel);
  mMeta->setID(id);
  if (isKernel) {
    mMeta->printHeader(this, O, mKernelName);
    mMeta->processArgMetadata(O, mBuffer, isKernel);
    mMeta->printGroupSize(O);
    mMeta->printDecls(this, O);
    kernel &tmp = const_cast<kernel&>(mGlobal->getKernel(mKernelName));
    // add the literals for the offsets and sizes of
    // all kernel declared local arrays
    if (tmp.lvgv) {
      localArg *lptr = tmp.lvgv;
      llvm::SmallVector<arraymem*, DEFAULT_VEC_SLOTS>::iterator lmb, lme;
      for (lmb = lptr->local.begin(), lme = lptr->local.end();
          lmb != lme; ++lmb) {
        mMFI->addi32Literal((*lmb)->offset);
        mMFI->addi32Literal((*lmb)->vecSize);
        mMFI->setUsesLocal();
      }
    }
    // Add the literals for the offsets and sizes of
    // all the globally scoped constant arrays
    for (StringMap<constPtr>::iterator cmb = mGlobal->consts_begin(),
        cme = mGlobal->consts_end(); cmb != cme; ++cmb) {
      mMFI->addi32Literal((cmb)->second.offset);
      mMFI->addi32Literal((cmb)->second.size);
      mMFI->addMetadata(";memory:datareqd");
    }

    // Add the literals for the offsets and sizes of
    // all the kernel constant arrays
    llvm::SmallVector<constPtr, DEFAULT_VEC_SLOTS>::const_iterator cpb, cpe;
    for (cpb = tmp.constPtr.begin(), cpe = tmp.constPtr.end();
        cpb != cpe; ++cpb) {
      mMFI->addi32Literal(cpb->size);
      mMFI->addi32Literal(cpb->offset);
    }
    mMeta->emitLiterals(O);
    // Add 1 to the size so that the next literal is the one we want
    mMeta->printArgCopies(O, this);
    O << "call " << id << " ; " << mName << "\n";
    mMeta->printFooter(O);
    mMeta->printMetaData(O, id, isKernel);
    O << "func " << id << " ; " << mName << "\n";
  } else {
    if (mName.empty()) {
      std::stringstream ss;
      ss << "unknown_" << id;
      mName = ss.str();
    }
    mMeta->setName(mName);
    O << "func " << id << " ; " << mName << "\n";
    mMeta->processArgMetadata(O, mBuffer, false);
  }
  O.flush();
  OutStreamer.EmitRawText(O.str());
}
  void
AMDILAsmPrinter::EmitFunctionBodyEnd()
{
  SmallString<1024> Str;
  raw_svector_ostream O(Str);
  uint32_t id = mName.empty() 
    ? mGlobal->getOrCreateFunctionID(MF->getFunction())
        : mGlobal->getOrCreateFunctionID(mName);
  if (mName.empty()) {
    std::stringstream ss;
    ss << "unknown_" << id;
    mName = ss.str();
  }
  if (mGlobal->isKernel(mKernelName)) {
    O << "endfunc ; " << mName << "\n";
    mMeta->setName(mName);
    mMeta->printMetaData(O, id, false);
  } else {
    O << "endfunc ; " << mName << "\n";
    mMeta->printMetaData(O, id, false);
  }
  mMeta->clear();
  O << ";DEBUGSTART\n";
  O.flush();
  OutStreamer.EmitRawText(O.str());
}
  void
AMDILAsmPrinter::EmitConstantPool()
{
  // If we aren't a kernel, we should not need to
  // emit constant pool data yet
  if (!mGlobal->isKernel(mKernelName)) {
    return;
  }
  kernel &tmp = const_cast<kernel&>(mGlobal->getKernel(mKernelName));
  mGlobal->calculateCPOffsets(MF, tmp);
  // Add all the constant pool offsets to the literal table
  for (uint32_t x = 0; x < tmp.CPOffsets.size(); ++x) {
    mMFI->addMetadata(";memory:datareqd");
    mMFI->addi32Literal(tmp.CPOffsets[x].first);
  }

  // Add all the constant pool constants to the literal tables
  {
    const MachineConstantPool *MCP = MF->getConstantPool();
    const std::vector<MachineConstantPoolEntry> &consts
      = MCP->getConstants();
    for (uint32_t x = 0, s = consts.size(); x < s; ++x) {
      addCPoolLiteral(consts[x].Val.ConstVal);
    }
  }
}
  void
AMDILAsmPrinter::EmitFunctionEntryLabel()
{
  return;
  assert(0 && "When is this function hit!");
}

bool
AMDILAsmPrinter::isMacroCall(const MachineInstr *MI) {
  return !strncmp(MI->getDesc().getName(), "MACRO", 5);
}

bool
AMDILAsmPrinter::isMacroFunc(const MachineInstr *MI) {
  if (MI->getOpcode() != AMDIL::CALL) {
    return false;
  }
  if (!MI->getOperand(0).isGlobal()) {
    return false;
  }
  const llvm::StringRef &nameRef = MI->getOperand(0).getGlobal()->getName();
  if (nameRef.startswith("__atom_")
      || nameRef.startswith("__atomic_")) {
    mMeta->setOutputInst();
  }
  return amd::MacroDBFindMacro(nameRef.data()) != -1;
}
  void
AMDILAsmPrinter::emitMCallInst(const MachineInstr *MI, OSTREAM_TYPE &O, const char *name)
{
  const AMDILSubtarget *curTarget = mTM->getSubtargetImpl();
  int macronum = amd::MacroDBFindMacro(name);
  int numIn = amd::MacroDBNumInputs(macronum);
  int numOut = amd::MacroDBNumOutputs(macronum);
  if (macronum == -1) {
    return;
  }
  if (curTarget->device()->isSupported(
        AMDILDeviceInfo::MacroDB)) {
    mMacroIDs.insert(macronum);
  } else {
    mMFI->addCalledIntr(macronum);
  }
  const TargetRegisterClass *trc = MF->getTarget()
    .getRegisterInfo()->getRegClass(AMDIL::GPRF32RegClassID);
  O << "\tmcall(" << macronum << ")(";
  int x ;
  for (x = 0; x < numOut - 1; ++x) {
    O << getRegisterName(trc->getRegister(x)) << ", ";
  }
  O << getRegisterName(trc->getRegister(x)) << "),(";
  for (x = 0; x < numIn - 1; ++x) {
    O << getRegisterName(trc->getRegister(x)) << ", ";
  }
  O << getRegisterName(trc->getRegister(x)) << ")";
  O << " ;" << name <<"\n";
}

