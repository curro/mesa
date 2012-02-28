#include <iostream>
#include <llvm-c/Core.h>
// #include "llvm/CodeGen/Function.h"
#include "OpenCLUtils.h"
#include "KernelParameters.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Support/TypeBuilder.h"
#include "llvm/Constants.h"
#include "llvm/Intrinsics.h"

#include "AMDIL.h"

using namespace llvm;
using namespace std;

#define CONSTANT_CACHE_SIZE_DW 127

class KernelParameters : public llvm::FunctionPass
{
  const llvm::TargetData * TD;
  LLVMContext* Context;
  Module *mod;
  struct param
  {
    param() : val(NULL), ptr_val(NULL), offset_in_dw(0), size_in_dw(0), specialID(0) {}
    
    llvm::Value* val;
    llvm::Value* ptr_val;
    int offset_in_dw;
    int size_in_dw;

    string specialType;
    int specialID;
    
    int end() { return offset_in_dw + size_in_dw; }
  };

  std::vector<param> params;

  int getLastSpecialID(const string& TypeName);
  
  int getListSize();
  void AddParam(llvm::Argument* arg);
  int calculateArgumentSize(llvm::Argument* arg);
  void RunAna(llvm::Function* fun);
  void Replace(llvm::Function* fun);
  void Propagete(llvm::Function* fun);
  void Propagete(llvm::Value* v, const llvm::Twine& name, bool indirect = false);
  Value* ConstantRead(Function* fun, param& p);
  Value* handleSpecial(Function* fun, param& p);
  bool isSpecialType(Type*);
  string getSpecialTypeName(Type*);
public:
  static char ID;
  KernelParameters() : FunctionPass(ID) {};
  KernelParameters(const llvm::TargetData* TD) : FunctionPass(ID), TD(TD) {}
//   bool runOnFunction (llvm::Function &F);
  bool runOnFunction (llvm::Function &F);
  void getAnalysisUsage(AnalysisUsage &AU) const;
  const char *getPassName() const;
  bool doInitialization(Module &M);
  bool doFinalization(Module &M);
};

char KernelParameters::ID = 0;

static RegisterPass<KernelParameters> X("kerparam", "OpenCL Kernel Parameter conversion", false, false);

int KernelParameters::getLastSpecialID(const string& TypeName)
{
  int lastID = -1;
  
  for (vector<param>::iterator i = params.begin(); i != params.end(); i++)
  {
    if (i->specialType == TypeName)
    {
      lastID = i->specialID;
    }
  }

  return lastID;
}

int KernelParameters::getListSize()
{
  if (params.size() == 0)
  {
    return 0;
  }

  return params.back().end();
}

void KernelParameters::AddParam(llvm::Argument* arg)
{
  param p;
  
  p.val = dyn_cast<Value>(arg);
  p.offset_in_dw = getListSize();
  p.size_in_dw = calculateArgumentSize(arg);

  params.push_back(p);
}

int KernelParameters::calculateArgumentSize(llvm::Argument* arg)
{
  Type* t = arg->getType();

  if (arg->hasByValAttr() and dyn_cast<PointerType>(t))
  {
    t = dyn_cast<PointerType>(t)->getElementType();
  }
  
  int store_size_in_dw = (TD->getTypeStoreSize(t) + 3)/4;

  assert(store_size_in_dw);
  
  return store_size_in_dw;
}


void KernelParameters::RunAna(llvm::Function* fun)
{
  assert(isOpenCLKernel(fun));

  for (Function::arg_iterator i = fun->arg_begin(); i != fun->arg_end(); i++)
  {
    AddParam(i);
  }

  cout << "Paramsize: " << getListSize() << " dwords" << endl;
}

void KernelParameters::Replace(llvm::Function* fun)
{
  for (std::vector<param>::iterator i = params.begin(); i != params.end(); i++)
  {
    Value *new_val;

    if (isSpecialType(i->val->getType()))
    {
      new_val = handleSpecial(fun, *i);
    }
    else
    {
      new_val = ConstantRead(fun, *i);
    }
    
    if (new_val)
    {
      i->val->replaceAllUsesWith(new_val);
    }   
  }
}

void KernelParameters::Propagete(llvm::Function* fun)
{
  for (std::vector<param>::iterator i = params.begin(); i != params.end(); i++)
  {
    if (i->ptr_val)
    {
      Propagete(i->ptr_val, i->val->getName());
    }
  }
}

void KernelParameters::Propagete(Value* v, const Twine& name, bool indirect)
{
  LoadInst* load = dyn_cast<LoadInst>(v);
  unsigned addrspace = llvm::AMDILAS::PARAM_D_ADDRESS;

  if (indirect)
  {
    addrspace = llvm::AMDILAS::PARAM_I_ADDRESS;
  }
  
  if (load)
  {
//     cout << "LOAD found: ";
//     load->dump();
//     cout << " addrspace: " << load->getPointerAddressSpace() << endl;

    if (load->getPointerAddressSpace() != addrspace)
    {
      Value *orig_ptr = load->getPointerOperand();
      PointerType *orig_ptr_type = dyn_cast<PointerType>(orig_ptr->getType());
      
      Type* new_ptr_type = PointerType::get(orig_ptr_type->getElementType(), addrspace);

      Value* new_ptr = orig_ptr;
      
      if (orig_ptr->getType() != new_ptr_type)
      {
        new_ptr = new BitCastInst(orig_ptr, new_ptr_type, "prop_cast", load);
      }
      
      Value* new_load = new LoadInst(new_ptr, name, load);
      load->replaceAllUsesWith(new_load);
      load->eraseFromParent();
    }
    
    return;
  }

  for (Value::use_iterator i = v->use_begin(); i != v->use_end(); i++)
  {
    GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(*i);
    Value* v2 = dyn_cast<Value>(*i);

    cout << "propagate: " << v2 << endl;
    
    if (GEP)
    {
      GetElementPtrInst::op_iterator i = GEP->op_begin();
      
      for (i++; i != GEP->op_end(); i++)
      {
        if (!isa<Constant>(*i))
        {
          indirect = true;
        }
      }
    }
      
    if (v2)
    {
      Propagete(v2, name, indirect);
    }
  }
}

Value* KernelParameters::ConstantRead(Function* fun, param& p)
{
  assert(fun->front().begin() != fun->front().end());
  
  Instruction *first_inst = fun->front().begin();

  if (!p.val->hasNUsesOrMore(1))
  {
    return NULL;
  }
  
  int addrspace = llvm::AMDILAS::PARAM_D_ADDRESS;
  
  Argument *arg = dyn_cast<Argument>(p.val);
  
  if (dyn_cast<PointerType>(p.val->getType()) and arg->hasByValAttr())
  {
    Value* ptr = new IntToPtrInst(ConstantInt::get(Type::getInt32Ty(*Context), p.offset_in_dw*4), PointerType::get(dyn_cast<PointerType>(p.val->getType())->getElementType(), addrspace), p.val->getName(), first_inst);
    
    p.ptr_val = ptr;
    return ptr;
  }
  else
  {
    Value* ptr = new IntToPtrInst(ConstantInt::get(Type::getInt32Ty(*Context), p.offset_in_dw*4), PointerType::get(p.val->getType(), addrspace), "", first_inst);
    
    p.ptr_val = ptr;
    return new LoadInst(ptr, p.val->getName(), first_inst);
  }
}

Value* KernelParameters::handleSpecial(Function* fun, param& p)
{
  string name = getSpecialTypeName(p.val->getType());
  int ID;

  assert(!name.empty());
  
  if (name == "image2d_t" or name == "image3d_t")
  {
    int lastID = max(getLastSpecialID("image2d_t"), getLastSpecialID("image3d_t"));
    
    if (lastID == -1)
    {
      ID = 2; ///ID0 and ID1 are used internally by the driver
    }
    else
    {
      ID = lastID + 1;
    }
  }
  else if (name == "sampler_t")
  {
    int lastID = getLastSpecialID("sampler_t");

    if (lastID == -1)
    {
      ID = 0;
    }
    else
    {
      ID = lastID + 1;
    }    
  }
  else
  {
    ///TODO: give some error message
    return NULL;
  }
    
  p.specialType = name;
  p.specialID = ID;

  Instruction *first_inst = fun->front().begin();

  return new IntToPtrInst(ConstantInt::get(Type::getInt32Ty(*Context), p.specialID), p.val->getType(), "resourceID", first_inst);
}


bool KernelParameters::isSpecialType(Type* t)
{
  return !getSpecialTypeName(t).empty();
}

string KernelParameters::getSpecialTypeName(Type* t)
{
  PointerType *pt = dyn_cast<PointerType>(t);
  StructType *st = NULL;

  if (pt)
  {
    st = dyn_cast<StructType>(pt->getElementType());
  }

  if (st)
  {
    string prefix = "struct.opencl_builtin_type_";
    
    string name = st->getName().str();

    if (name.substr(0, prefix.length()) == prefix)
    {
      return name.substr(prefix.length(), name.length());
    }
  }

  return "";
}


bool KernelParameters::runOnFunction (Function &F)
{
  if (!isOpenCLKernel(&F))
  {
    cout << "notkernel" << endl;
    return false;
  }

  F.dump();
  
  RunAna(&F);
  Replace(&F);
  Propagete(&F);
  
  F.dump();
  return false;
}

void KernelParameters::getAnalysisUsage(AnalysisUsage &AU) const
{
//   AU.addRequired<FunctionAnalysis>();
  FunctionPass::getAnalysisUsage(AU);
  AU.setPreservesAll();
}

const char *KernelParameters::getPassName() const
{
  return "OpenCL Kernel parameter conversion to memory";
}

bool KernelParameters::doInitialization(Module &M)
{
  Context = &M.getContext();
  mod = &M;
  
  return false;
}

bool KernelParameters::doFinalization(Module &M)
{
  return false;
}

llvm::FunctionPass* createKernelParametersPass(const llvm::TargetData* TD)
{
  FunctionPass *p = new KernelParameters(TD);
  
  return p;
}


