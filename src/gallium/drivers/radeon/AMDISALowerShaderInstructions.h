
#ifndef AMDISA_LOWER_SHADER_INSTRUCTIONS
#define AMDISA_LOWER_SHADER_INSTRUCTIONS

namespace llvm {

class MachineRegisterInfo;

class AMDISALowerShaderInstructionsPass {

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


#endif // AMDISA_LOWER_SHADER_INSTRUCTIONS
