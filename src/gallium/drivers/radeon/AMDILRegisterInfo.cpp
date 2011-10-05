//===- AMDILRegisterInfo.cpp - AMDIL Register Information -------*- C++ -*-===//
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
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the AMDIL implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "AMDILRegisterInfo.h"
#include "AMDIL.h"
#include "AMDILUtilityFunctions.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;

AMDILRegisterInfo::AMDILRegisterInfo(AMDILTargetMachine &tm,
    const TargetInstrInfo &tii)
: AMDILGenRegisterInfo(0), // RA???
  TM(tm), TII(tii)
{
  baseOffset = 0;
  nextFuncOffset = 0;
}

const unsigned*
AMDILRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const
{
  static const unsigned CalleeSavedRegs[] = { 0 };
  // TODO: Does IL need to actually have any callee saved regs?
  // I don't think we do since we can just use sequential registers
  // Maybe this would be easier if every function call was inlined first
  // and then there would be no callee issues to deal with
  //TODO(getCalleeSavedRegs);
  return CalleeSavedRegs;
}

BitVector
AMDILRegisterInfo::getReservedRegs(const MachineFunction &MF) const
{
  BitVector Reserved(getNumRegs());
  // We reserve the first getNumRegs() registers as they are the ones passed
  // in live-in/live-out
  // and therefor cannot be killed by the scheduler. This works around a bug
  // discovered
  // that was causing the linearscan register allocator to kill registers
  // inside of the
  // function that were also passed as LiveIn registers.
  for (unsigned int x = 0, y = 256; x < y; ++x) {
    Reserved.set(x);
  }
  return Reserved;
}

BitVector
AMDILRegisterInfo::getAllocatableSet(const MachineFunction &MF,
    const TargetRegisterClass *RC = NULL) const
{
  BitVector Allocatable(getNumRegs());
  Allocatable.clear();
  return Allocatable;
}

const TargetRegisterClass* const*
AMDILRegisterInfo::getCalleeSavedRegClasses(const MachineFunction *MF) const
{
  static const TargetRegisterClass * const CalleeSavedRegClasses[] = { 0 };
  // TODO: Keep in sync with getCalleeSavedRegs
  //TODO(getCalleeSavedRegClasses);
  return CalleeSavedRegClasses;
}
#if LLVM_VERSION < 2500
bool
AMDILRegisterInfo::hasFP(const MachineFunction &MF) const
{
  //TODO(hasFP);
  return false;
}
#endif
void
AMDILRegisterInfo::eliminateCallFramePseudoInstr(
    MachineFunction &MF,
    MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const
{
  MBB.erase(I);
}

// For each frame index we find, we store the offset in the stack which is
// being pushed back into the global buffer. The offset into the stack where
// the value is stored is copied into a new register and the frame index is
// then replaced with that register.
#if LLVM_VERSION < 2500
unsigned int
#else
void 
#endif
AMDILRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
    int SPAdj, 
#if LLVM_VERSION < 2500
    FrameIndexValue *Value, 
#endif
    RegScavenger *RS) const
{
  assert(SPAdj == 0 && "Unexpected");
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  MachineFrameInfo *MFI = MF.getFrameInfo();
  unsigned int y = MI.getNumOperands();
  for (unsigned int x = 0; x < y; ++x) {
    if (!MI.getOperand(x).isFI()) {
      continue;
    }
    bool def = isStoreInst(&MI);
    int FrameIndex = MI.getOperand(x).getIndex();
    int64_t Offset = MFI->getObjectOffset(FrameIndex);
    //int64_t Size = MF.getFrameInfo()->getObjectSize(FrameIndex);
    // An optimization is to only use the offsets if the size
    // is larger than 4, which means we are storing an array
    // instead of just a pointer. If we are size 4 then we can
    // just do register copies since we don't need to worry about
    // indexing dynamically
    MachineInstr *nMI = MF.CreateMachineInstr(
        TII.get(AMDIL::LOADCONST_i32), MI.getDebugLoc());
    nMI->addOperand(MachineOperand::CreateReg(AMDIL::DFP, true));
    nMI->addOperand(
        MachineOperand::CreateImm(Offset));
    MI.getParent()->insert(II, nMI);
    nMI = MF.CreateMachineInstr(
        TII.get(AMDIL::ADD_i32), MI.getDebugLoc());
    nMI->addOperand(MachineOperand::CreateReg(AMDIL::DFP, true));
    nMI->addOperand(MachineOperand::CreateReg(AMDIL::DFP, false));
    nMI->addOperand(MachineOperand::CreateReg(AMDIL::FP, false));
    
    MI.getParent()->insert(II, nMI);
    if (MI.getOperand(x).isReg() == false)  {
      MI.getOperand(x).ChangeToRegister(
          nMI->getOperand(0).getReg(), def);
    } else {
      MI.getOperand(x).setReg(
          nMI->getOperand(0).getReg());
    }
  }
#if LLVM_VERSION < 2500
  return 0;
#endif
}

void
AMDILRegisterInfo::processFunctionBeforeFrameFinalized(
    MachineFunction &MF) const
{
  //TODO(processFunctionBeforeFrameFinalized);
  // Here we keep track of the amount of stack that the current function
  // uses so
  // that we can set the offset to the end of the stack and any other
  // function call
  // will not overwrite any stack variables.
  // baseOffset = nextFuncOffset;
  MachineFrameInfo *MFI = MF.getFrameInfo();

  for (uint32_t x = 0, y = MFI->getNumObjects(); x < y; ++x) {
    int64_t size = MFI->getObjectSize(x);
    if (!(size % 4) && size > 1) {
      nextFuncOffset += size;
    } else {
      nextFuncOffset += 16;
    }
  }
}
#if LLVM_VERSION < 2500
void
AMDILRegisterInfo::emitPrologue(MachineFunction &MF) const
{
  //TODO(emitPrologue);
}

void
AMDILRegisterInfo::emitEpilogue(
    MachineFunction &MF,
    MachineBasicBlock &MBB) const
{
  //TODO(emitEpilogue);
}
#endif
unsigned int
AMDILRegisterInfo::getRARegister() const
{
  return AMDIL::RA;
}

unsigned int
AMDILRegisterInfo::getFrameRegister(const MachineFunction &MF) const
{
  return AMDIL::FP;
}

unsigned int
AMDILRegisterInfo::getEHExceptionRegister() const
{
  assert(0 && "What is the exception register");
  return 0;
}

unsigned int
AMDILRegisterInfo::getEHHandlerRegister() const
{
  assert(0 && "What is the exception handler register");
  return 0;
}

int64_t
AMDILRegisterInfo::getStackSize() const
{
  return nextFuncOffset - baseOffset;
}

#define GET_REGINFO_MC_DESC
#define GET_REGINFO_TARGET_DESC
#include "AMDILGenRegisterInfo.inc"

