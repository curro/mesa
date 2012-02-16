/*
 * Copyright 2011 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors: Tom Stellard <thomas.stellard@amd.com>
 *
 */


#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

#include "AMDIL.h"
#include "AMDGPU.h"
#include "AMDGPULowerShaderInstructions.h"
#include "AMDILInstrInfo.h"

#include <vector>

using namespace llvm;

namespace {
  class R600LowerShaderInstructionsPass : public MachineFunctionPass,
        public AMDGPULowerShaderInstructionsPass {

  private:
    static char ID;
    TargetMachine &TM;

    void lowerEXPORT_REG_FAKE(MachineInstr &MI, MachineBasicBlock &MBB,
        MachineBasicBlock::iterator I);
    void lowerLOAD_INPUT(MachineInstr & MI);
    bool lowerSTORE_OUTPUT(MachineInstr & MI, MachineBasicBlock &MBB,
        MachineBasicBlock::iterator I);

  public:
    R600LowerShaderInstructionsPass(TargetMachine &tm) :
      MachineFunctionPass(ID), TM(tm) { }

      bool runOnMachineFunction(MachineFunction &MF);

      const char *getPassName() const { return "R600 Lower Shader Instructions"; }
    };
} /* End anonymous namespace */

char R600LowerShaderInstructionsPass::ID = 0;

FunctionPass *llvm::createR600LowerShaderInstructionsPass(TargetMachine &tm) {
    return new R600LowerShaderInstructionsPass(tm);
}

#define INSTR_CASE_FLOAT_V(inst) \
  case AMDIL:: inst##_v4f32: \

#define INSTR_CASE_FLOAT_S(inst) \
  case AMDIL:: inst##_f32:

#define INSTR_CASE_FLOAT(inst) \
  INSTR_CASE_FLOAT_V(inst) \
  INSTR_CASE_FLOAT_S(inst)
bool R600LowerShaderInstructionsPass::runOnMachineFunction(MachineFunction &MF)
{
  MRI = &MF.getRegInfo();


  for (MachineFunction::iterator BB = MF.begin(), BB_E = MF.end();
                                                  BB != BB_E; ++BB) {
    MachineBasicBlock &MBB = *BB;
    for (MachineBasicBlock::iterator I = MBB.begin(); I != MBB.end();) {
      MachineInstr &MI = *I;
      bool deleteInstr = false;
      switch (MI.getOpcode()) {

      default: break;

      case AMDIL::RESERVE_REG:
      case AMDIL::EXPORT_REG:
        deleteInstr = true;
        break;

      case AMDIL::LOAD_INPUT:
        lowerLOAD_INPUT(MI);
        deleteInstr = true;
        break;

      case AMDIL::MOV:
        BuildMI(MBB, I, MBB.findDebugLoc(I), TM.getInstrInfo()->get(AMDIL::COPY))
                .addOperand(MI.getOperand(0))
                .addOperand(MI.getOperand(1));
        deleteInstr = true;
        break;

      case AMDIL::STORE_OUTPUT:
        deleteInstr = lowerSTORE_OUTPUT(MI, MBB, I);
        break;

      }

      ++I;

      if (deleteInstr) {
        MI.eraseFromParent();
      }
    }
  }

  MRI->EmitLiveInCopies(MF.begin(), *TM.getRegisterInfo(), *TM.getInstrInfo());

//  MF.dump();
  return false;
}

/* The goal of this function is to replace the virutal destination register of
 * a LOAD_INPUT instruction with the correct physical register that will.
 *
 * XXX: I don't think this is the right way things assign physical registers,
 * but I'm not sure of another way to do this.
 */
void R600LowerShaderInstructionsPass::lowerLOAD_INPUT(MachineInstr &MI)
{
  MachineOperand &dst = MI.getOperand(0);
  MachineOperand &arg = MI.getOperand(1);
  int64_t inputIndex = arg.getImm();
  const TargetRegisterClass * inputClass = TM.getRegisterInfo()->getRegClass(AMDIL::R600_TReg32RegClassID);
  unsigned newRegister = inputClass->getRegister(inputIndex);
  unsigned dstReg = dst.getReg();

  preloadRegister(newRegister, dstReg);
}

bool R600LowerShaderInstructionsPass::lowerSTORE_OUTPUT(MachineInstr &MI,
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I)
{
  MachineOperand &valueOp = MI.getOperand(1);
  MachineOperand &indexOp = MI.getOperand(2);
  unsigned valueReg = valueOp.getReg();
  int64_t outputIndex = indexOp.getImm();
  const TargetRegisterClass * outputClass = TM.getRegisterInfo()->getRegClass(AMDIL::R600_TReg32RegClassID);
  unsigned newRegister = outputClass->getRegister(outputIndex);

  BuildMI(MBB, I, MBB.findDebugLoc(I), TM.getInstrInfo()->get(AMDIL::COPY),
                  newRegister)
                  .addReg(valueReg);

  if (!MRI->isLiveOut(newRegister))
    MRI->addLiveOut(newRegister);

  return true;

}
