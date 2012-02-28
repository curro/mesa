#ifndef KERNELPARAMETERS_H
#define KERNELPARAMETERS_H

#include <vector>
#include "llvm/Function.h"
#include "llvm/Value.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Pass.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

llvm::FunctionPass* createKernelParametersPass(const llvm::TargetData* TD);


#endif
