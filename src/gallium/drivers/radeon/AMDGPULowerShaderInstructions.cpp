
#include "llvm/CodeGen/MachineRegisterInfo.h"

#include "AMDGPULowerShaderInstructions.h"

using namespace llvm;

void AMDGPULowerShaderInstructionsPass::preloadRegister(
    unsigned physReg, unsigned virtReg) const
{
  if (!MRI->isLiveIn(physReg)) {
    MRI->addLiveIn(physReg, virtReg);
  } else {
    /* We can't mark the same register as preloaded twice, but we still must
     * associate virtReg with the correct preloaded register. */
    unsigned newReg = MRI->getLiveInVirtReg(physReg);
    MRI->replaceRegWith(virtReg, newReg);
  }
}
