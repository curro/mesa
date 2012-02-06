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
#include "AMDISA.h"
#include "AMDISAInstrInfo.h"
#include "AMDISAUtil.h"

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

    virtual bool runOnMachineFunction(MachineFunction &MF);

  };
} /* End anonymous namespace */

char R600LowerInstructionsPass::ID = 0;

FunctionPass *llvm::createR600LowerInstructionsPass(TargetMachine &tm) {
  return new R600LowerInstructionsPass(tm);
}

bool R600LowerInstructionsPass::runOnMachineFunction(MachineFunction &MF)
{
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
        uint32_t zero = (uint32_t)APFloat(0.0f).bitcastToAPInt().getZExtValue();
        uint32_t one = (uint32_t)APFloat(1.0f).bitcastToAPInt().getZExtValue();
        uint32_t low = getLiteral(MFI, MI.getOperand(2).getImm());
        uint32_t high = getLiteral(MFI, MI.getOperand(3).getImm());
        if (low == zero && high == one) {
          MachineInstr *def = NULL;
          /* Even though we are in SSA, it is possible for a register to have
           * more than one def.  This occurs when an instruction writes to an
           * output register that has also been used as an input register.
           * This is only an issue when dealing with graphics shaders. */
          for (MachineRegisterInfo::def_iterator DI =
               MRI.def_begin(MI.getOperand(1).getReg()), DE = MRI.def_end();
                DI != DE; ++DI) {
            def = &*DI;
            if (!isPlaceHolderOpcode((&*DI)->getOpcode())) {
              def = &*DI;
              break;
            }
          }
          assert(def);
          MI.getOperand(0).addTargetFlag(MO_FLAG_CLAMP);
          BuildMI(MBB, I, MBB.findDebugLoc(I),
                          TII->get(TII->getISAOpcode(AMDIL::MOVE_f32)))
                  .addOperand(MI.getOperand(0))
                  .addReg(def->getOperand(0).getReg());
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
      case AMDIL::LOADCONST_f32:
      case AMDIL::LOADCONST_i32:
        {
        bool canDelete = true;
        MachineOperand * use = MI.getOperand(0).getNextOperandForReg();
        while (use) {
          MachineOperand * next = use->getNextOperandForReg();
          /* XXX: assert(next->isUse()) */
          /* XXX: Having immediates in MOV instructions (maybe others) causes
           * the register allocator to elminate them when there are IF
           * statements.  I'm not sure why this is happening, so for now we only
           * propogate immediates to when they are needed by CLAMP instructions.
           */
          if (use->getParent()->getOpcode() != AMDIL::CLAMP_f32) {
            canDelete = false;
            break;
          } else {
            use->ChangeToImmediate(MI.getOperand(1).getImm());
          }
          use = next;
        }
        if (!canDelete) {
          continue;
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
