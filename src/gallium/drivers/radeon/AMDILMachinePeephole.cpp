//===-- AMDILMachinePeephole.cpp - AMDIL Machine Peephole Pass -*- C++ -*-===//
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


#define DEBUG_TYPE "machine_peephole"
#if !defined(NDEBUG)
#define DEBUGME (DebugFlag && isCurrentDebugType(DEBUG_TYPE))
#else
#define DEBUGME (false)
#endif

#include "AMDIL.h"
#include "AMDILSubtarget.h"
#include "AMDILUtilityFunctions.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetMachine.h"


using namespace llvm;
namespace
{
  class AMDILMachinePeephole : public MachineFunctionPass
  {
    public:
      static char ID;
      AMDILMachinePeephole(TargetMachine &tm AMDIL_OPT_LEVEL_DECL);
      //virtual ~AMDILMachinePeephole();
      virtual const char*
        getPassName() const;
      virtual bool
        runOnMachineFunction(MachineFunction &MF);
    private:
      void insertFence(MachineBasicBlock::iterator &MIB);
      TargetMachine &TM;
      bool mDebug;
  }; // AMDILMachinePeephole
  char AMDILMachinePeephole::ID = 0;
} // anonymous namespace

namespace llvm
{
  FunctionPass*
    createAMDILMachinePeephole(TargetMachine &tm AMDIL_OPT_LEVEL_DECL)
    {
      return new AMDILMachinePeephole(tm AMDIL_OPT_LEVEL_VAR);
    }
} // llvm namespace

AMDILMachinePeephole::AMDILMachinePeephole(TargetMachine &tm AMDIL_OPT_LEVEL_DECL)
#if LLVM_VERSION >= 2500
  : MachineFunctionPass(ID), TM(tm)
#else
  : MachineFunctionPass((intptr_t)&ID), TM(tm)
#endif
{
  mDebug = DEBUGME;
}

bool
AMDILMachinePeephole::runOnMachineFunction(MachineFunction &MF)
{
  bool Changed = false;
  const AMDILSubtarget *STM = &TM.getSubtarget<AMDILSubtarget>();
  for (MachineFunction::iterator MBB = MF.begin(), MBE = MF.end();
      MBB != MBE; ++MBB) {
    MachineBasicBlock *mb = MBB;
    for (MachineBasicBlock::iterator MIB = mb->begin(), MIE = mb->end();
        MIB != MIE; ++MIB) {
      MachineInstr *mi = MIB;
      switch (mi->getOpcode()) {
        default:
          if (isAtomicInst(mi)) {
            // If we don't support the hardware accellerated address spaces,
            // then the atomic needs to be transformed to the global atomic.
            if (strstr(mi->getDesc().getName(), "_L_")
                && STM->device()->usesSoftware(AMDILDeviceInfo::LocalMem)) {
              BuildMI(*mb, MIB, mi->getDebugLoc(), 
                  TM.getInstrInfo()->get(AMDIL::ADD_i32), AMDIL::R1011)
                .addReg(mi->getOperand(1).getReg())
                .addReg(AMDIL::T2);
              mi->getOperand(1).setReg(AMDIL::R1011);
              mi->setDesc(
                  TM.getInstrInfo()->get(
                    (mi->getOpcode() - AMDIL::ATOM_L_ADD) + AMDIL::ATOM_G_ADD));
            } else if (strstr(mi->getDesc().getName(), "_R_")
                && STM->device()->usesSoftware(AMDILDeviceInfo::RegionMem)) {
              assert(!"Software region memory is not supported!");
              mi->setDesc(
                  TM.getInstrInfo()->get(
                    (mi->getOpcode() - AMDIL::ATOM_R_ADD) + AMDIL::ATOM_G_ADD));
            }
          } else if ((isLoadInst(mi) || isStoreInst(mi)) && isVolatileInst(mi)) {
            insertFence(MIB);
          }
          continue;
          break;
        case AMDIL::USHR_i16:
        case AMDIL::USHR_v2i16:
        case AMDIL::USHR_v4i16:
        case AMDIL::USHRVEC_i16:
        case AMDIL::USHRVEC_v2i16:
        case AMDIL::USHRVEC_v4i16:
          if (TM.getSubtarget<AMDILSubtarget>()
              .device()->usesSoftware(AMDILDeviceInfo::ShortOps)) {
            unsigned lReg = MF.getRegInfo()
              .createVirtualRegister(&AMDIL::GPRI32RegClass);
            unsigned Reg = MF.getRegInfo()
              .createVirtualRegister(&AMDIL::GPRV4I32RegClass);
            BuildMI(*mb, MIB, mi->getDebugLoc(),
                TM.getInstrInfo()->get(AMDIL::LOADCONST_i32),
                lReg).addImm(0xFFFF);
            BuildMI(*mb, MIB, mi->getDebugLoc(),
                TM.getInstrInfo()->get(AMDIL::BINARY_AND_v4i32),
                Reg)
              .addReg(mi->getOperand(1).getReg())
              .addReg(lReg);
            mi->getOperand(1).setReg(Reg);
          }
          break;
        case AMDIL::USHR_i8:
        case AMDIL::USHR_v2i8:
        case AMDIL::USHR_v4i8:
        case AMDIL::USHRVEC_i8:
        case AMDIL::USHRVEC_v2i8:
        case AMDIL::USHRVEC_v4i8:
          if (TM.getSubtarget<AMDILSubtarget>()
              .device()->usesSoftware(AMDILDeviceInfo::ByteOps)) {
            unsigned lReg = MF.getRegInfo()
              .createVirtualRegister(&AMDIL::GPRI32RegClass);
            unsigned Reg = MF.getRegInfo()
              .createVirtualRegister(&AMDIL::GPRV4I32RegClass);
            BuildMI(*mb, MIB, mi->getDebugLoc(),
                TM.getInstrInfo()->get(AMDIL::LOADCONST_i32),
                lReg).addImm(0xFF);
            BuildMI(*mb, MIB, mi->getDebugLoc(),
                TM.getInstrInfo()->get(AMDIL::BINARY_AND_v4i32),
                Reg)
              .addReg(mi->getOperand(1).getReg())
              .addReg(lReg);
            mi->getOperand(1).setReg(Reg);
          }
          break;
      }
    }
  }
  return Changed;
}

const char*
AMDILMachinePeephole::getPassName() const
{
  return "AMDIL Generic Machine Peephole Optimization Pass";
}

void
AMDILMachinePeephole::insertFence(MachineBasicBlock::iterator &MIB)
{
  MachineInstr *MI = MIB;
  MachineInstr *fence = BuildMI(*(MI->getParent()->getParent()),
        MI->getDebugLoc(),
        TM.getInstrInfo()->get(AMDIL::FENCE)).addReg(1);

  MI->getParent()->insert(MIB, fence);
  fence = BuildMI(*(MI->getParent()->getParent()),
        MI->getDebugLoc(),
        TM.getInstrInfo()->get(AMDIL::FENCE)).addReg(1);
  MIB = MI->getParent()->insertAfter(MIB, fence);
}
