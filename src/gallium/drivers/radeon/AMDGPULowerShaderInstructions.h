
#ifndef AMDGPU_LOWER_SHADER_INSTRUCTIONS
#define AMDGPU_LOWER_SHADER_INSTRUCTIONS

namespace llvm {

class MachineRegisterInfo;

class AMDGPULowerShaderInstructionsPass {

  protected:
    MachineRegisterInfo * MRI;
    /**
     * @param physReg The physical register that will be preloaded.
     * @param virtReg The virtual register that currently holds the
     *                preloaded value.
     */
    void preloadRegister(unsigned physReg, unsigned virtReg) const;
};

} // end namespace llvm


#endif // AMDGPU_LOWER_SHADER_INSTRUCTIONS
