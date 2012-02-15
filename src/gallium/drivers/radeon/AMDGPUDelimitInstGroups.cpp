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


#include "AMDIL.h"
#include "AMDGPU.h"
#include "AMDGPURegisterInfo.h"
#include "AMDGPUUtil.h"

#include "llvm/ADT/IndexedMap.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetInstrInfo.h"

#include <iostream>
#include <stdio.h>
#include <map>
using namespace llvm;

namespace {

  class AMDGPUDelimitInstGroupsPass : public MachineFunctionPass {

  private:
  static char ID;
  TargetMachine &TM;
  int currentLast;
  std::map<unsigned,bool> constantReads;
  /* The key for this map is a register index, and its value are MachineInstr
   * pointers.  This map keeps track of register uses whose defintions have already
   * been added to the current instruction group, and therefore cannot be
   * emitted in the current group. */
  IndexedMap<MachineInstr*> invalidUses;

  void endGroup(MachineBasicBlock &BB, MachineFunction &MF,
    MachineBasicBlock::iterator lastInst);

  void clearALUUnits();

  void addConstantReads(MachineInstr &MI);

  public:
  AMDGPUDelimitInstGroupsPass(TargetMachine &tm) :
    MachineFunctionPass(ID), TM(tm), currentLast(-1) { }

  virtual bool runOnMachineFunction(MachineFunction &MF);
  };
} /* End anonymous namespace */

char AMDGPUDelimitInstGroupsPass::ID = 0;

FunctionPass *llvm::createAMDGPUDelimitInstGroupsPass(TargetMachine &tm) {
  return new AMDGPUDelimitInstGroupsPass(tm);
}

bool AMDGPUDelimitInstGroupsPass::runOnMachineFunction(MachineFunction &MF)
{
//  MF.dump();
  const AMDGPURegisterInfo * TRI =
                  static_cast<const AMDGPURegisterInfo*>(TM.getRegisterInfo());
  for (MachineFunction::iterator BB = MF.begin(), BB_E = MF.end();
                                                  BB != BB_E; ++BB) {
    MachineBasicBlock &MBB = *BB;
    MachineBasicBlock::iterator lastRealInst = MBB.begin();
    for (MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end();
                                                      I != E; ++I) {
      MachineInstr &MI = *I;

      if (MI.getOpcode() == AMDIL::EXPORT_REG) {
        endGroup(MBB, MF, lastRealInst);
        continue;
      }

      if (isReductionOp(MI.getOpcode())) {
        endGroup(MBB, MF, lastRealInst);
        endGroup(MBB, MF, I);
        continue;
      }

/*      if (isFCOp(MI.getOpcode())) {
        endGroup(MBB, MF, lastRealInst);
        continue;
      }
*/
      if (isPlaceHolderOpcode(MI.getOpcode()) || MI.getNumOperands() == 0) {
        continue;
      }

      MachineOperand dstOp = MI.getOperand(0);
      if (!dstOp.isReg()) {
        continue;
      }
      unsigned dstReg = dstOp.getReg();

      /* Only 4 different constant registers are allowed to be read per
       * instruction group */
      addConstantReads(MI);

      /* XXX: This is assuming the register is of type f32 */
      unsigned element;
      if (MI.getOpcode() == AMDIL::SET_CHAN) {
        element = MI.getOperand(2).getImm();
      } else {
        element = getRegElement(TRI, dstReg);
      }
      if (currentLast > -1 && element <= (unsigned)currentLast) {
        endGroup(MBB, MF, lastRealInst);
      } else if (constantReads.size() > 4){
        endGroup(MBB, MF, lastRealInst);
      } else {
        for (unsigned i = 1, numOps = MI.getNumOperands(); i < numOps; i++) {
          if (!MI.getOperand(i).isReg()) {
            continue;
          }
          unsigned reg = MI.getOperand(i).getReg();
          if (invalidUses.inBounds(reg) && invalidUses[reg]) {
            endGroup(MBB, MF, lastRealInst);
            break;
          }
        }
      }

      if (isTransOp(MI.getOpcode())) {
        endGroup(MBB, MF, I);
        continue;
      }

      /* XXX; getNextOperandForReg() appears to go in the reverse order of what I
       * would expect.  It finds the previous use, not the next one. */
      MachineOperand * nextUse = dstOp.getNextOperandForReg();
      if (nextUse) {
        invalidUses.grow(dstReg);
        invalidUses[dstReg] = nextUse->getParent();
      }
      /* Update the current last element */
      currentLast = element;

      /* We need to add the constant reads again in case they were cleared
       * by the endGroup() function. */
      addConstantReads(MI);

      lastRealInst = I;
    }
    endGroup(MBB, MF, lastRealInst);
  }
  return false;
}

void AMDGPUDelimitInstGroupsPass::endGroup(MachineBasicBlock &BB,
    MachineFunction &MF, MachineBasicBlock::iterator lastInst)
{
  currentLast = -1;
  constantReads.clear();
  invalidUses.clear();
  /* XXX: Enum here */
  BB.insertAfter(lastInst, BuildMI(MF, BB.findDebugLoc(lastInst),
                                TM.getInstrInfo()->get(AMDIL::LAST)));
}

void AMDGPUDelimitInstGroupsPass::addConstantReads(MachineInstr &MI)
{
  for (unsigned i = 1; i < MI.getNumOperands(); ++i) {
    MachineOperand MO = MI.getOperand(i);
    if (!MO.isReg()) {
      continue;
    }
    if (AMDIL::R600_CReg_32RegClass.contains(MO.getReg())) {
      constantReads[MO.getReg()] = true;
    }
  }
}
