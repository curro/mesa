//===--- AMDILLiteralManager.cpp - AMDIL Literal Manager Pass --*- C++ -*--===//
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

#define DEBUG_TYPE "literal_manager"
#include "AMDIL.h"
#include "AMDILAlgorithms.tpp"
#include "AMDILKernelManager.h"
#include "AMDILMachineFunctionInfo.h"
#include "AMDILSubtarget.h"
#include "AMDILTargetMachine.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;


// AMDIL Literal Manager traverses through all of the LOADCONST instructions and
// converts them from an immediate value to the literal index. The literal index
// is valid IL, but the immediate values are not. The Immediate values must be
// aggregated and declared for clarity and to reduce the number of literals that
// are used. It is also illegal to declare the same literal twice, so this keeps
// that from occuring.

namespace {
  class AMDILLiteralManager : public MachineFunctionPass {
  public:
    static char ID;
    AMDILLiteralManager(TargetMachine &tm, CodeGenOpt::Level OL);
    virtual const char *getPassName() const;

    bool runOnMachineFunction(MachineFunction &MF);
  private:
    bool trackLiterals(MachineBasicBlock::iterator *bbb);
    TargetMachine &TM;
    const AMDILSubtarget *mSTM;
    AMDILKernelManager *mKM;
    AMDILMachineFunctionInfo *mMFI;
    int32_t mLitIdx;
    bool mChanged;
  };
  char AMDILLiteralManager::ID = 0;
}

namespace llvm {
  FunctionPass *
  createAMDILLiteralManager(TargetMachine &tm, CodeGenOpt::Level OL) {
    return new AMDILLiteralManager(tm, OL);
  }
  
}

AMDILLiteralManager::AMDILLiteralManager(TargetMachine &tm,
                                         CodeGenOpt::Level OL)
#if LLVM_VERSION >= 2500
  : MachineFunctionPass(ID),
#else
  : MachineFunctionPass((intptr_t)&ID),
#endif
    TM(tm) {
}

bool AMDILLiteralManager::runOnMachineFunction(MachineFunction &MF) {
  mChanged = false;
  mMFI = MF.getInfo<AMDILMachineFunctionInfo>();
  const AMDILTargetMachine *amdtm =
    reinterpret_cast<const AMDILTargetMachine *>(&TM);
  mSTM = dynamic_cast<const AMDILSubtarget *>(amdtm->getSubtargetImpl());
  mKM = const_cast<AMDILKernelManager *>(mSTM->getKernelManager());
  safeNestedForEach(MF.begin(), MF.end(), MF.begin()->begin(),
      std::bind1st(std::mem_fun(&AMDILLiteralManager::trackLiterals), this));
  return mChanged;
}

bool AMDILLiteralManager::trackLiterals(MachineBasicBlock::iterator *bbb) {
  MachineInstr *MI = *bbb;
  uint32_t Opcode = MI->getOpcode();
  switch(Opcode) {
  default:
    return false;
  case AMDIL::LOADCONST_i8:
  case AMDIL::LOADCONST_i16:
  case AMDIL::LOADCONST_i32:
  case AMDIL::LOADCONST_i64:
  case AMDIL::LOADCONST_f32:
  case AMDIL::LOADCONST_f64:
    break;
  };
  MachineOperand &dstOp = MI->getOperand(0);
  MachineOperand &litOp = MI->getOperand(1);
  if (!litOp.isImm() && !litOp.isFPImm()) {
    return false;
  }
  if (!dstOp.isReg()) {
    return false;
  }
  // Change the literal to the correct index for each literal that is found.
  if (litOp.isImm()) {
    int64_t immVal = litOp.getImm();
    uint32_t idx = MI->getOpcode() == AMDIL::LOADCONST_i64 
                     ? mMFI->addi64Literal(immVal)
                     : mMFI->addi32Literal(static_cast<int>(immVal), Opcode);
    litOp.ChangeToImmediate(idx);
    return false;
  } 

  if (litOp.isFPImm()) {
    const ConstantFP *fpVal = litOp.getFPImm();
    uint32_t idx = MI->getOpcode() == AMDIL::LOADCONST_f64
                     ? mMFI->addf64Literal(fpVal)
                     : mMFI->addf32Literal(fpVal);
    litOp.ChangeToImmediate(idx);
    return false;
  }

  return false;
}

const char* AMDILLiteralManager::getPassName() const {
    return "AMDIL Constant Propagation";
}


