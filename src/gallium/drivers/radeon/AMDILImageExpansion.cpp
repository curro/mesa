//
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
// @file AMDILImageExpansion.cpp
// @details Implementatino of the Image expansion class for image capable devices
//
#include "AMDILIOExpansion.h"
#include "AMDILKernelManager.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Value.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/Support/DebugLoc.h"
#include "llvm/Target/TargetInstrInfo.h"
using namespace llvm;

AMDILImageExpansion::AMDILImageExpansion(TargetMachine &tm AMDIL_OPT_LEVEL_DECL)
  : AMDIL789IOExpansion(tm AMDIL_OPT_LEVEL_VAR)
{
}

AMDILImageExpansion::~AMDILImageExpansion()
{
}
void AMDILImageExpansion::expandInefficientImageLoad(
    MachineBasicBlock *mBB, MachineInstr *MI)
{
#if 0
  const llvm::StringRef &name = MI->getOperand(0).getGlobal()->getName();
  const char *tReg1, *tReg2, *tReg3, *tReg4;
  tReg1 = mASM->getRegisterName(MI->getOperand(1).getReg());
  if (MI->getOperand(2).isReg()) {
    tReg2 = mASM->getRegisterName(MI->getOperand(2).getReg());
  } else {
    tReg2 = mASM->getRegisterName(AMDIL::R1);
    O << "\tmov " << tReg2 << ", l" << MI->getOperand(2).getImm() << "\n";
  }
  if (MI->getOperand(3).isReg()) {
    tReg3 = mASM->getRegisterName(MI->getOperand(3).getReg());
  } else {
    tReg3 = mASM->getRegisterName(AMDIL::R2);
    O << "\tmov " << tReg3 << ", l" << MI->getOperand(3).getImm() << "\n";
  }
  if (MI->getOperand(4).isReg()) {
    tReg4 = mASM->getRegisterName(MI->getOperand(4).getReg());
  } else {
    tReg4 = mASM->getRegisterName(AMDIL::R3);
    O << "\tmov " << tReg2 << ", l" << MI->getOperand(4).getImm() << "\n";
  }
  bool internalSampler = false;
  //bool linear = true;
  unsigned ImageCount = 3; // OPENCL_MAX_READ_IMAGES
  unsigned SamplerCount = 3; // OPENCL_MAX_SAMPLERS
  if (ImageCount - 1) {
  O << "\tswitch " << mASM->getRegisterName(MI->getOperand(1).getReg())
    << "\n";
  }
  for (unsigned rID = 0; rID < ImageCount; ++rID) {
    if (ImageCount - 1)  {
    if (!rID) {
      O << "\tdefault\n";
    } else {
      O << "\tcase " << rID << "\n" ;
    }
    O << "\tswitch " << mASM->getRegisterName(MI->getOperand(2).getReg())
     << "\n";
    }
    for (unsigned sID = 0; sID < SamplerCount; ++sID) {
      if (SamplerCount - 1) {
      if (!sID) {
        O << "\tdefault\n";
      } else {
        O << "\tcase " << sID << "\n" ;
      }
      }
      if (internalSampler) {
        // Check if sampler has normalized setting.
        O << "\tand r0.x, " << tReg2 << ".x, l0.y\n"
          << "\tif_logicalz r0.x\n"
          << "\tflr " << tReg3 << ", " << tReg3 << "\n"
          << "\tsample_resource(" << rID << ")_sampler("
          << sID << ")_coordtype(unnormalized) "
          << tReg1 << ", " << tReg3 << " ; " << name.data() << "\n"
          << "\telse\n"
          << "\tiadd " << tReg1 << ".y, " << tReg1 << ".x, l0.y\n"
          << "\titof " << tReg2 << ", cb1[" << tReg1 << ".x].xyz\n"
          << "\tmul " << tReg3 << ", " << tReg3 << ", " << tReg2 << "\n"
          << "\tflr " << tReg3 << ", " << tReg3 << "\n"
          << "\tmul " << tReg3 << ", " << tReg3 << ", cb1[" 
          << tReg1 << ".y].xyz\n"
          << "\tsample_resource(" << rID << ")_sampler("
          << sID << ")_coordtype(normalized) "
          << tReg1 << ", " << tReg3 << " ; " << name.data() << "\n"
          << "\tendif\n";
      } else {
        O << "\tiadd " << tReg1 << ".y, " << tReg1 << ".x, l0.y\n"
          // Check if sampler has normalized setting.
          << "\tand r0, " << tReg2 << ".x, l0.y\n"
          // Convert image dimensions to float.
          << "\titof " << tReg4 << ", cb1[" << tReg1 << ".x].xyz\n"
          // Move into R0 1 if unnormalized or dimensions if normalized.
          << "\tcmov_logical r0, r0, " << tReg4 << ", r1.1111\n"
          // Make coordinates unnormalized.
          << "\tmul " << tReg3 << ", r0, " << tReg3 << "\n"
          // Get linear filtering if set.
          << "\tand " << tReg4 << ", " << tReg2 << ".x, l6.x\n"
          // Save unnormalized coordinates in R0.
          << "\tmov r0, " << tReg3 << "\n"
          // Floor the coordinates due to HW incompatibility with precision
          // requirements.
          << "\tflr " << tReg3 << ", " << tReg3 << "\n"
          // get Origianl coordinates (without floor) if linear filtering
          << "\tcmov_logical " << tReg3 << ", " << tReg4 
          << ".xxxx, r0, " << tReg3 << "\n"
          // Normalize the coordinates with multiplying by 1/dimensions
          << "\tmul " << tReg3 << ", " << tReg3 << ", cb1[" 
          << tReg1 << ".y].xyz\n"
          << "\tsample_resource(" << rID << ")_sampler("
          << sID << ")_coordtype(normalized) "
          << tReg1 << ", " << tReg3 << " ; " << name.data() << "\n";
      }
      if (SamplerCount - 1) {
      O << "\tbreak\n";
      }
    }
    if (SamplerCount - 1) {
      O << "\tendswitch\n";
    }
    if (ImageCount - 1) {
    O << "\tbreak\n";
    }
  }
  if (ImageCount - 1) {
    O << "\tendswitch\n";
  }
#endif
}
  void
AMDILImageExpansion::expandImageLoad(MachineBasicBlock *mBB, MachineInstr *MI)
{
  uint32_t imageID = getPointerID(MI);
  MI->getOperand(1).ChangeToImmediate(imageID);
  saveInst = true;
}
  void
AMDILImageExpansion::expandImageStore(MachineBasicBlock *mBB, MachineInstr *MI)
{
  uint32_t imageID = getPointerID(MI);
  mKM->setOutputInst();
  MI->getOperand(0).ChangeToImmediate(imageID);
  saveInst = true;
}
  void
AMDILImageExpansion::expandImageParam(MachineBasicBlock *mBB, MachineInstr *MI)
{
    MachineBasicBlock::iterator I = *MI;
    uint32_t ID = getPointerID(MI);
    DebugLoc DL = MI->getDebugLoc();
    BuildMI(*mBB, I, DL, mTII->get(AMDIL::CBLOAD), 
        MI->getOperand(0).getReg())
        .addImm(ID)
        .addImm(1);
}
