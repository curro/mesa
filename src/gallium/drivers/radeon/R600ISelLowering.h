//===-- R600ISelLowering.h - TODO: Add brief description -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// TODO: Add full description
//
//===----------------------------------------------------------------------===//

#ifndef R600ISELLOWERING_H
#define R600ISELLOWERING_H

#include "AMDGPUISelLowering.h"

namespace llvm {

class R600InstrInfo;

class R600TargetLowering : public AMDGPUTargetLowering
{
public:
  R600TargetLowering(TargetMachine &TM);
  virtual MachineBasicBlock * EmitInstrWithCustomInserter(MachineInstr *MI,
      MachineBasicBlock * BB) const;

private:
  const R600InstrInfo * TII;

  void lowerImplicitParameter(MachineInstr *MI, MachineBasicBlock &BB,
      MachineRegisterInfo & MRI, unsigned dword_offset) const;

};

} // End namespace llvm;

#endif // R600ISELLOWERING_H
