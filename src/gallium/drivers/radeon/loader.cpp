
#include "radeon_llvm.h"

#include "OpenCLUtils.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <stdio.h>

#include <llvm-c/Core.h>

using namespace llvm;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));



int main(int argc, char ** argv)
{
	unsigned char * bytes;
	unsigned byte_count;

	std::auto_ptr<Module> M;
	LLVMContext &Context = getGlobalContext();
	SMDiagnostic Err;
	cl::ParseCommandLineOptions(argc, argv, "llvm system compiler\n");
	M.reset(ParseIRFile(InputFilename, Err, Context));

	Module * mod = M.get();
  
// Value *Elts[] = {
//     MDString::get(mod->getContext(), "test1")
//   };
//   
//   MDNode *Node = MDNode::get(getGlobalContext(), Elts);
// 
//   NamedMDNode * md = mod->getOrInsertNamedMetadata("opencl.kernels");
// 
//   
//   printf("%s %i\n", md->getName().str().c_str(), md->getNumOperands());
//   printf("%i\n", md->getOperand(0)->getNumOperands());
// 
//   for (int i = 0; i < 2; i++)
//   {
//   printf("%s\n", md->getOperand(i)->getOperand(0)->getName().str().c_str());
//   printf("%u\n", md->getOperand(i)->getOperand(0)->getValueID());
//   }
//   
//   md->addOperand(Node);

  for (Module::iterator i = mod->begin(); i != mod->end(); i++)
  {
    printf("%s %i\n", i->getName().str().c_str(), isOpenCLKernel(i));
  }
  
//   printf("...........\n");
// 	mod->dump();
//   return 0;
  printf("...........\n");
	radeon_llvm_compile(wrap(mod), &bytes, &byte_count, "cayman", 1);
}
