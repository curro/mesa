//===----- AMDILBarrierDetect.cpp - Barrier Detect pass -*- C++ -*- ------===//
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

#define DEBUG_TYPE "BarrierDetect"
#ifdef DEBUG
#define DEBUGME (DebugFlag && isCurrentDebugType(DEBUG_TYPE))
#else
#define DEBUGME 0
#endif
#include "AMDILAlgorithms.tpp"
#include "AMDILDevices.h"
#include "AMDILCompilerWarnings.h"
#include "AMDILMachineFunctionInfo.h"
#include "AMDILSubtarget.h"
#include "AMDILTargetMachine.h"
#include "llvm/BasicBlock.h"
#include "llvm/Instructions.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunctionAnalysis.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Target/TargetMachine.h"
using namespace llvm;

// The barrier detect pass determines if a barrier has been duplicated in the
// source program which can cause undefined behaviour if more than a single
// wavefront is executed in a group. This is because LLVM does not have an
// execution barrier and if this barrier function gets duplicated, undefined
// behaviour can occur. In order to work around this, we detect the duplicated
// barrier and then make the work-group execute in a single wavefront mode,
// essentially making the barrier a no-op.

namespace
{
  class LLVM_LIBRARY_VISIBILITY AMDILBarrierDetect : public FunctionPass
  {
    TargetMachine &TM;
    static char ID;
  public:
    AMDILBarrierDetect(TargetMachine &TM AMDIL_OPT_LEVEL_DECL);
    ~AMDILBarrierDetect();
    const char *getPassName() const;
    bool runOnFunction(Function &F);
    bool doInitialization(Module &M);
    bool doFinalization(Module &M);
    void getAnalysisUsage(AnalysisUsage &AU) const;
  private:
    bool detectBarrier(BasicBlock::iterator *BBI);
    bool detectMemFence(BasicBlock::iterator *BBI);
    bool mChanged;
    SmallVector<int64_t, DEFAULT_VEC_SLOTS> bVecMap;
    const AMDILSubtarget *mStm;

    // Constants used to define memory type.
    static const unsigned int LOCAL_MEM_FENCE = 1<<0;
    static const unsigned int GLOBAL_MEM_FENCE = 1<<1;
    static const unsigned int REGION_MEM_FENCE = 1<<2;
  };
  char AMDILBarrierDetect::ID = 0;
} // anonymouse namespace

namespace llvm
{
  FunctionPass *
  createAMDILBarrierDetect(TargetMachine &TM AMDIL_OPT_LEVEL_DECL)
  {
    return new AMDILBarrierDetect(TM  AMDIL_OPT_LEVEL_VAR);
  }
} // llvm namespace

AMDILBarrierDetect::AMDILBarrierDetect(TargetMachine &TM
                                       AMDIL_OPT_LEVEL_DECL)
  : 
#if LLVM_VERSION >= 2500
  FunctionPass(ID),
#else
  FunctionPass((intptr_t)&ID),
#endif
  TM(TM)
{
}

AMDILBarrierDetect::~AMDILBarrierDetect()
{
}

bool AMDILBarrierDetect::detectBarrier(BasicBlock::iterator *BBI)
{
  SmallVector<int64_t, DEFAULT_VEC_SLOTS>::iterator bIter;
  int64_t bID;
  Instruction *inst = (*BBI);
  CallInst *CI = dyn_cast<CallInst>(inst);

  if (!CI || !CI->getNumOperands()) {
    return false;
  }
#if LLVM_VERSION < 2500
  const Value *funcVal = CI->getOperand(0);
#else
  const Value *funcVal = CI->getOperand(CI->getNumOperands() - 1);
#endif
  if (funcVal && strncmp(funcVal->getName().data(), "__amd_barrier", 13)) {
    return false;
  }

  if (inst->getNumOperands() >= 3) {
#if LLVM_VERSION < 2500
    const Value *V = inst->getOperand(1);
#else
    const Value *V = inst->getOperand(0);
#endif
    const ConstantInt *Cint = dyn_cast<ConstantInt>(V);
    bID = Cint->getSExtValue();
    bIter = std::find(bVecMap.begin(), bVecMap.end(), bID);
    if (bIter == bVecMap.end()) {
      bVecMap.push_back(bID);
    } else {
      if (mStm->device()->isSupported(AMDILDeviceInfo::BarrierDetect)) {
        AMDILMachineFunctionInfo *MFI =
          getAnalysis<MachineFunctionAnalysis>().getMF()
          .getInfo<AMDILMachineFunctionInfo>();
        MFI->addMetadata(";limitgroupsize");
        MFI->addErrorMsg(amd::CompilerWarningMessage[BAD_BARRIER_OPT]);
      }
    }
  }
  if (mStm->device()->getGeneration() == AMDILDeviceInfo::HD4XXX) {
    AMDILMachineFunctionInfo *MFI =
      getAnalysis<MachineFunctionAnalysis>().getMF()
          .getInfo<AMDILMachineFunctionInfo>();
    MFI->addErrorMsg(amd::CompilerWarningMessage[LIMIT_BARRIER]);
    MFI->addMetadata(";limitgroupsize");
    MFI->setUsesLocal();
  }
#if LLVM_VERSION < 2500
  const Value *V = inst->getOperand(inst->getNumOperands()-1);
  const ConstantInt *Cint = dyn_cast<ConstantInt>(V);
  Function *iF = dyn_cast<Function>(inst->getOperand(0));
#else
  const Value *V = inst->getOperand(inst->getNumOperands()-2);
  const ConstantInt *Cint = dyn_cast<ConstantInt>(V);
  Function *iF = dyn_cast<Function>(inst->getOperand(inst->getNumOperands()-1));
#endif
  Module *M = iF->getParent();
  bID = Cint->getSExtValue();
  if (bID > 0) {
    const char *name = "barrier";
    if (bID == GLOBAL_MEM_FENCE) {
      name = "barrierGlobal";
    } else if (bID == LOCAL_MEM_FENCE
        && mStm->device()->usesHardware(AMDILDeviceInfo::LocalMem)) {
      name = "barrierLocal";
    } else if (bID == REGION_MEM_FENCE
               && mStm->device()->usesHardware(AMDILDeviceInfo::RegionMem)) {
      name = "barrierRegion";
    }
    Function *nF =
      dyn_cast<Function>(M->getOrInsertFunction(name, iF->getFunctionType()));
#if LLVM_VERSION < 2500
    inst->setOperand(0, nF);
#else
    inst->setOperand(inst->getNumOperands()-1, nF);
#endif
    return false;
  }

  return false;
}

bool AMDILBarrierDetect::detectMemFence(BasicBlock::iterator *BBI)
{
  int64_t bID;
  Instruction *inst = (*BBI);
  CallInst *CI = dyn_cast<CallInst>(inst);

  if (!CI || CI->getNumOperands() != 2) {
    return false;
  }

#if LLVM_VERSION < 2500
  const Value *V = inst->getOperand(inst->getNumOperands()-1);
  const ConstantInt *Cint = dyn_cast<ConstantInt>(V);
  Function *iF = dyn_cast<Function>(inst->getOperand(0));
#else
  const Value *V = inst->getOperand(inst->getNumOperands()-2);
  const ConstantInt *Cint = dyn_cast<ConstantInt>(V);
  Function *iF = dyn_cast<Function>(inst->getOperand(inst->getNumOperands()-1));
#endif

  const char *fence_local_name;
  const char *fence_global_name;
  const char *fence_region_name;
  const char* fence_name = "mem_fence";
  if (!iF) {
    return false;
  }

  if (strncmp(iF->getName().data(), "mem_fence", 9) == 0) {
    fence_local_name = "mem_fence_local";
    fence_global_name = "mem_fence_global";
    fence_region_name = "mem_fence_region";
  } else if (strncmp(iF->getName().data(), "read_mem_fence", 14) == 0) {
    fence_local_name = "read_mem_fence_local";
    fence_global_name = "read_mem_fence_global";
    fence_region_name = "read_mem_fence_region";
  } else if (strncmp(iF->getName().data(), "write_mem_fence", 15) == 0) {
    fence_local_name = "write_mem_fence_local";
    fence_global_name = "write_mem_fence_global";
    fence_region_name = "write_mem_fence_region";
  } else {
    return false;
  }

  Module *M = iF->getParent();
  bID = Cint->getSExtValue();
  if (bID > 0) {
    const char *name = fence_name;
    if (bID == GLOBAL_MEM_FENCE) {
      name = fence_global_name;
    } else if (bID == LOCAL_MEM_FENCE
        && mStm->device()->usesHardware(AMDILDeviceInfo::LocalMem)) {
      name = fence_local_name;
    } else if (bID == REGION_MEM_FENCE
               && mStm->device()->usesHardware(AMDILDeviceInfo::RegionMem)) {
      name = fence_region_name;
    }
    Function *nF =
      dyn_cast<Function>(M->getOrInsertFunction(name, iF->getFunctionType()));
#if LLVM_VERSION < 2500
    inst->setOperand(0, nF);
#else
    inst->setOperand(inst->getNumOperands()-1, nF);
#endif
    return false;
  }

  return false;

}

bool AMDILBarrierDetect::runOnFunction(Function &MF)
{
  mChanged = false;
  bVecMap.clear();
  mStm = &TM.getSubtarget<AMDILSubtarget>();
  Function *F = &MF;
  safeNestedForEach(F->begin(), F->end(), F->begin()->begin(),
      std::bind1st(
        std::mem_fun(
          &AMDILBarrierDetect::detectBarrier), this));
  safeNestedForEach(F->begin(), F->end(), F->begin()->begin(),
      std::bind1st(
        std::mem_fun(
          &AMDILBarrierDetect::detectMemFence), this));
  return mChanged;
}

const char* AMDILBarrierDetect::getPassName() const
{
  return "AMDIL Barrier Detect Pass";
}

bool AMDILBarrierDetect::doInitialization(Module &M)
{
  return false;
}

bool AMDILBarrierDetect::doFinalization(Module &M)
{
  return false;
}

void AMDILBarrierDetect::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addRequired<MachineFunctionAnalysis>();
  FunctionPass::getAnalysisUsage(AU);
  AU.setPreservesAll();
}
