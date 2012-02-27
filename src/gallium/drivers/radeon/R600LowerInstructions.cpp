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
#include "llvm/Constants.h"
#include "llvm/Target/TargetInstrInfo.h"

#include "AMDIL.h"
#include "AMDILMachineFunctionInfo.h"
#include "AMDILRegisterInfo.h"
#include "AMDGPU.h"
#include "AMDGPUInstrInfo.h"
#include "AMDGPUUtil.h"

#include "R600InstrInfo.h"

#include <stdio.h>

using namespace llvm;

namespace {
  class R600LowerInstructionsPass : public MachineFunctionPass {

  private:
    static char ID;
    TargetMachine &TM;
    AMDILMachineFunctionInfo * MFI;

    void lowerFLT(MachineInstr &MI);

  public:
    R600LowerInstructionsPass(TargetMachine &tm) :
      MachineFunctionPass(ID), TM(tm) { }

    const char *getPassName() const { return "R600 Lower Instructions"; }
    virtual bool runOnMachineFunction(MachineFunction &MF);

  };
} /* End anonymous namespace */

char R600LowerInstructionsPass::ID = 0;

FunctionPass *llvm::createR600LowerInstructionsPass(TargetMachine &tm) {
  return new R600LowerInstructionsPass(tm);
}

bool R600LowerInstructionsPass::runOnMachineFunction(MachineFunction &MF)
{
  MF.dump();
  MachineRegisterInfo & MRI = MF.getRegInfo();
  MFI = MF.getInfo<AMDILMachineFunctionInfo>();
  const R600InstrInfo * TII =
                        static_cast<const R600InstrInfo*>(TM.getInstrInfo());

  for (MachineFunction::iterator BB = MF.begin(), BB_E = MF.end();
                                                  BB != BB_E; ++BB) {
    MachineBasicBlock &MBB = *BB;
    for (MachineBasicBlock::iterator I = MBB.begin(), Next = llvm::next(I);
         I != MBB.end(); I = Next, Next = llvm::next(I) ) {
      MachineInstr &MI = *I;

      switch(MI.getOpcode()) {
      case AMDIL::FLT:
        BuildMI(MBB, I, MBB.findDebugLoc(I), TM.getInstrInfo()->get(AMDIL::FGE))
                .addOperand(MI.getOperand(0))
                .addOperand(MI.getOperand(2))
                .addOperand(MI.getOperand(1));
        break;

      /* XXX: We could propagate the ABS flag to all of the uses of Operand0 and
       * remove the ABS instruction.*/
      case AMDIL::FABS_f32:
      case AMDIL::ABS_f32:
        MI.getOperand(1).addTargetFlag(MO_FLAG_ABS);
        BuildMI(MBB, I, MBB.findDebugLoc(I), TM.getInstrInfo()->get(AMDIL::MOVE_f32))
                .addOperand(MI.getOperand(0))
                .addOperand(MI.getOperand(1));
        break;

      case AMDIL::BINARY_OR_f32:
        {
        unsigned tmp0 = MRI.createVirtualRegister(&AMDIL::GPRI32RegClass);
        BuildMI(MBB, I, MBB.findDebugLoc(I), TM.getInstrInfo()->get(AMDIL::FTOI), tmp0)
                .addOperand(MI.getOperand(1));
        unsigned tmp1 = MRI.createVirtualRegister(&AMDIL::GPRI32RegClass);
        BuildMI(MBB, I, MBB.findDebugLoc(I), TM.getInstrInfo()->get(AMDIL::FTOI), tmp1)
                .addOperand(MI.getOperand(2));
        unsigned tmp2 = MRI.createVirtualRegister(&AMDIL::GPRI32RegClass);
        BuildMI(MBB, I, MBB.findDebugLoc(I), TM.getInstrInfo()->get(AMDIL::BINARY_OR_i32), tmp2)
                .addReg(tmp0)
                .addReg(tmp1);
        BuildMI(MBB, I, MBB.findDebugLoc(I), TM.getInstrInfo()->get(AMDIL::ITOF), MI.getOperand(0).getReg())
                .addReg(tmp2);
        break;
        }
      case AMDIL::CMOVLOG_f32:
      case AMDIL::CMOVLOG_i32:
        BuildMI(MBB, I, MBB.findDebugLoc(I), TM.getInstrInfo()->get(MI.getOpcode()))
                .addOperand(MI.getOperand(0))
                .addOperand(MI.getOperand(1))
                .addOperand(MI.getOperand(3))
                .addOperand(MI.getOperand(2));
        break;

      case AMDIL::CLAMP_f32:
        {
          MachineOperand lowOp = MI.getOperand(2);
          MachineOperand highOp = MI.getOperand(3);
        if (lowOp.isReg() && highOp.isReg()
            && lowOp.getReg() == AMDIL::ZERO && highOp.getReg() == AMDIL::ONE) {
          MI.getOperand(0).addTargetFlag(MO_FLAG_CLAMP);
          BuildMI(MBB, I, MBB.findDebugLoc(I), TII->get(AMDIL::MOV))
                  .addOperand(MI.getOperand(0))
                  .addOperand(MI.getOperand(1));
        } else {
          /* XXX: Handle other cases */
          abort();
        }
        break;
        }
      /* XXX: Figure out the semantics of DIV_INF_f32 and make sure this is OK */
/*      case AMDIL::DIV_INF_f32:
        {
          unsigned tmp0 = MRI.createVirtualRegister(&AMDIL::GPRF32RegClass);
          BuildMI(MBB, I, MBB.findDebugLoc(I),
                          TM.getInstrInfo()->get(AMDIL::RECIP_CLAMPED), tmp0)
                  .addOperand(MI.getOperand(2));
          BuildMI(MBB, I, MBB.findDebugLoc(I),
                          TM.getInstrInfo()->get(AMDIL::MUL_IEEE_f32))
                  .addOperand(MI.getOperand(0))
                  .addReg(tmp0)
                  .addOperand(MI.getOperand(1));
          break;
        }
*/        /* XXX: This is an optimization */

      case AMDIL::GLOBALSTORE_i32:
        {
          unsigned index_reg =
                   MRI.createVirtualRegister(&AMDIL::R600_TReg32_XRegClass);
          BuildMI(MBB, I, MBB.findDebugLoc(I),
                          TII->get(AMDIL::MOV), index_reg)
                  .addReg(AMDIL::ZERO);
          /* XXX: Check GPU Family */
          BuildMI(MBB, I, MBB.findDebugLoc(I),
                          TII->get(AMDIL::RAT_WRITE_CACHELESS_eg))
                  .addOperand(MI.getOperand(0))
                  .addReg(index_reg)
                  .addImm(0);
          break;
        }
      case AMDIL::LOADCONST_f32:
      case AMDIL::LOADCONST_i32:
        {
          bool canInline = false;
          unsigned inlineReg;
          MachineOperand & dstOp = MI.getOperand(0);
          MachineOperand & immOp = MI.getOperand(1);
          if (immOp.isFPImm()) {
            const ConstantFP * cfp = immOp.getFPImm();
            if (cfp->isZero()) {
              canInline = true;
              inlineReg = AMDIL::ZERO;
            } else if (cfp->isExactlyValue(1.0f)) {
              canInline = true;
              inlineReg = AMDIL::ONE;
            } else if (cfp->isExactlyValue(0.5f)) {
              canInline = true;
              inlineReg = AMDIL::HALF;
            }
          }

          if (canInline) {
            MachineOperand * use = dstOp.getNextOperandForReg();
            /* The lowering operation for CLAMP needs to have the immediates
             * as operands, so we must propagate them. */
            while (use) {
              MachineOperand * next = use->getNextOperandForReg();
              if (use->getParent()->getOpcode() == AMDIL::CLAMP_f32) {
                use->setReg(inlineReg);
              }
              use = next;
            }
            BuildMI(MBB, I, MBB.findDebugLoc(I), TII->get(AMDIL::COPY))
                    .addOperand(dstOp)
                    .addReg(inlineReg);
          } else {
            BuildMI(MBB, I, MBB.findDebugLoc(I), TII->get(AMDIL::MOV))
                    .addOperand(dstOp)
                    .addReg(AMDIL::ALU_LITERAL_X)
                    .addOperand(immOp);
          }
          break;
        }

      case AMDIL::MASK_WRITE:
      {
        unsigned maskedRegister = MI.getOperand(0).getReg();
        assert(TargetRegisterInfo::isVirtualRegister(maskedRegister));
        MachineInstr * defInstr = MRI.getVRegDef(maskedRegister);
        MachineOperand * def = defInstr->findRegisterDefOperand(maskedRegister);
        def->addTargetFlag(MO_FLAG_MASK);
        break;
      }

      case AMDIL::VEXTRACT_v4f32:
        MI.getOperand(2).setImm(MI.getOperand(2).getImm() - 1);
        continue;

      case AMDIL::NEG_f32:
      case AMDIL::NEGATE_i32:
        {
            MI.getOperand(1).addTargetFlag(MO_FLAG_NEG);
            BuildMI(MBB, I, MBB.findDebugLoc(I),
                    TII->get(TII->getISAOpcode(AMDIL::MOV)))
            .addOperand(MI.getOperand(0))
            .addOperand(MI.getOperand(1));
          break;
        }

      case AMDIL::SUB_f32:
        {
          MI.getOperand(2).addTargetFlag(MO_FLAG_NEG);
          BuildMI(MBB, I, MBB.findDebugLoc(I),
                          TII->get(TII->getISAOpcode(AMDIL::ADD_f32)))
                  .addOperand(MI.getOperand(0))
                  .addOperand(MI.getOperand(1))
                  .addOperand(MI.getOperand(2));
          break;
        }

      case AMDIL::VINSERT_v4f32:
        {

          int64_t swz = MI.getOperand(4).getImm();
          int64_t chan;
          switch (swz) {
          case (1 << 0):
            chan = 0;
            break;
          case (1 << 8):
            chan = 1;
            break;
          case (1 << 16):
            chan = 2;
            break;
          case (1 << 24):
            chan = 3;
            break;
          default:
            chan = 0;
            fprintf(stderr, "swizzle: %ld\n", swz);
            abort();
            break;
          }
          BuildMI(MBB, I, MBB.findDebugLoc(I),
                          TM.getInstrInfo()->get(AMDIL::SET_CHAN))
                  .addOperand(MI.getOperand(1))
                  .addOperand(MI.getOperand(2))
                  .addImm(chan);

          BuildMI(MBB, I, MBB.findDebugLoc(I),
                                      TM.getInstrInfo()->get(AMDIL::COPY))
                  .addOperand(MI.getOperand(0))
                  .addOperand(MI.getOperand(1));
          break;
        }

      default:
        continue;
      }
      MI.eraseFromParent();
    }
  }
  return false;
}
