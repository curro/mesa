//===-- AMDILPeepholeOpt.cpp - Peephole Optimization pass --===//
// Copyright (c) 2011, Advanced Micro Devices, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// Neither the name of the copyright holder nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// If you use the software (in whole or in part), you shall adhere to all
// applicable U.S., European, and other export laws, including but not limited
// to the U.S. Export Administration Regulations (�EAR�), (15 C.F.R. Sections
// 730 through 774), and E.U. Council Regulation (EC) No 1334/2000 of 22 June
// 2000.  Further, pursuant to Section 740.6 of the EAR, you hereby certify
// that, except pursuant to a license granted by the United States Department
// of Commerce Bureau of Industry and Security or as otherwise permitted
// pursuant to a License Exception under the U.S. Export Administration
// Regulations ("EAR"), you will not (1) export, re-export or release to a
// national of a country in Country Groups D:1, E:1 or E:2 any restricted
// technology, software, or source code you receive hereunder, or (2) export to
// Country Groups D:1, E:1 or E:2 the direct product of such technology or
// software, if such foreign produced direct product is subject to national
// security controls as identified on the Commerce Control List (currently
// found in Supplement 1 to Part 774 of EAR).  For the most current Country
// Group listings, or for additional information about the EAR or your
// obligations under those regulations, please refer to the U.S. Bureau of
// Industry and Security�s website at http://www.bis.doc.gov/.
//
//==-----------------------------------------------------------------------===//

#define DEBUG_TYPE "PeepholeOpt"
#ifdef DEBUG
#define DEBUGME (DebugFlag && isCurrentDebugType(DEBUG_TYPE))
#else
#define DEBUGME 0
#endif

#include "AMDILAlgorithms.tpp"
#include "AMDILDevices.h"
#include "AMDILGlobalManager.h"
#include "AMDILKernelManager.h"
#include "AMDILMachineFunctionInfo.h"
#include "AMDILUtilityFunctions.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionAnalysis.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include <sstream>
#if 0
STATISTIC(PointerAssignments, "Number of dynamic pointer "
    "assigments discovered");
STATISTIC(PointerSubtract, "Number of pointer subtractions discovered");
#endif
STATISTIC(LocalFuncs, "Number of get_local_size(N) functions removed");

using namespace llvm;
// The Peephole optimization pass is used to do simple last minute optimizations
// that are required for correct code or to remove redundant functions
namespace {
class LLVM_LIBRARY_VISIBILITY AMDILPeepholeOpt : public FunctionPass {
public:
  TargetMachine &TM;
  static char ID;
  AMDILPeepholeOpt(TargetMachine &tm, CodeGenOpt::Level OL);
  ~AMDILPeepholeOpt();
  const char *getPassName() const;
  bool runOnFunction(Function &F);
  bool doInitialization(Module &M);
  bool doFinalization(Module &M);
  void getAnalysisUsage(AnalysisUsage &AU) const;
protected:
private:
  // Function to initiate all of the instruction level optimizations.
  bool instLevelOptimizations(BasicBlock::iterator *inst);
  // Quick check to see if we need to dump all of the pointers into the
  // arena. If this is correct, then we set all pointers to exist in arena. This
  // is a workaround for aliasing of pointers in a struct/union.
  bool dumpAllIntoArena(Function &F);
  // Because I don't want to invalidate any pointers while in the
  // safeNestedForEachFunction. I push atomic conversions to a vector and handle
  // it later. This function does the conversions if required.
  void doAtomicConversionIfNeeded(Function &F);
  // Because __amdil_is_constant cannot be properly evaluated if
  // optimizations are disabled, the call's are placed in a vector
  // and evaluated after the __amdil_image* functions are evaluated
  // which should allow the __amdil_is_constant function to be
  // evaluated correctly.
  void doIsConstCallConversionIfNeeded();
  bool mChanged;
  bool mDebug;
  bool mRWGOpt;
  bool mConvertAtomics;
  CodeGenOpt::Level optLevel;
  // Run a series of tests to see if we can optimize a CALL instruction.
  bool optimizeCallInst(BasicBlock::iterator *bbb);
  // A peephole optimization to optimize bit extract sequences.
  bool optimizeBitExtract(Instruction *inst);
  // A peephole optimization to optimize bit insert sequences.
  bool optimizeBitInsert(Instruction *inst);
  bool setupBitInsert(Instruction *base, 
                      Instruction *&src, 
                      Constant *&mask, 
                      Constant *&shift);
  // Expand the bit field insert instruction on versions of OpenCL that
  // don't support it.
  bool expandBFI(CallInst *CI);
  // Expand the bit field mask instruction on version of OpenCL that 
  // don't support it.
  bool expandBFM(CallInst *CI);
  // On 7XX and 8XX operations, we do not have 24 bit signed operations. So in
  // this case we need to expand them. These functions check for 24bit functions
  // and then expand.
  bool isSigned24BitOps(CallInst *CI);
  void expandSigned24BitOps(CallInst *CI);
  // One optimization that can occur is that if the required workgroup size is
  // specified then the result of get_local_size is known at compile time and
  // can be returned accordingly.
  bool isRWGLocalOpt(CallInst *CI);
  void expandRWGLocalOpt(CallInst *CI);
  // On northern island cards, the division is slightly less accurate than on
  // previous generations, so we need to utilize a more accurate division. So we
  // can translate the accurate divide to a normal divide on all other cards.
  bool convertAccurateDivide(CallInst *CI);
  void expandAccurateDivide(CallInst *CI);
  // If the alignment is set incorrectly, it can produce really inefficient
  // code. This checks for this scenario and fixes it if possible.
  bool correctMisalignedMemOp(Instruction *inst);

  // If we are in no opt mode, then we need to make sure that
  // local samplers are properly propagated as constant propagation 
  // doesn't occur and we need to know the value of kernel defined
  // samplers at compile time.
  bool propagateSamplerInst(CallInst *CI);

  LLVMContext *mCTX;
  Function *mF;
  const AMDILSubtarget *mSTM;
  SmallVector< std::pair<CallInst *, Function *>, 16> atomicFuncs;
  SmallVector<CallInst *, 16> isConstVec;
}; // class AMDILPeepholeOpt
  char AMDILPeepholeOpt::ID = 0;
} // anonymous namespace

namespace llvm {
  FunctionPass *
  createAMDILPeepholeOpt(TargetMachine &tm, CodeGenOpt::Level OL) 
  {
    return new AMDILPeepholeOpt(tm, OL);
  }
} // llvm namespace

AMDILPeepholeOpt::AMDILPeepholeOpt(TargetMachine &tm, CodeGenOpt::Level OL)
#if LLVM_VERSION >= 2500
  : FunctionPass(ID), TM(tm) 
#else
  : FunctionPass((intptr_t)&ID), TM(tm) 
#endif
{
  mDebug = DEBUGME;
  optLevel = OL;
}

AMDILPeepholeOpt::~AMDILPeepholeOpt() 
{
}

const char *
AMDILPeepholeOpt::getPassName() const 
{
  return "AMDIL PeepHole Optimization Pass";
}

bool 
containsPointerType(Type *Ty) 
{
  if (!Ty) {
    return false;
  }
  switch(Ty->getTypeID()) {
  default:
    return false;
#if LLVM_VERSION < 2500
  case Type::UnionTyID: {
    const UnionType *ST = dyn_cast<UnionType>(Ty);
    for (UnionType::element_iterator stb = ST->element_begin(),
           ste = ST->element_end(); stb != ste; ++stb) {
      if (!containsPointerType(*stb)) {
        continue;
      }
      return true;
    }
    break;
  }
#endif
  case Type::StructTyID: {
    const StructType *ST = dyn_cast<StructType>(Ty);
    for (StructType::element_iterator stb = ST->element_begin(),
           ste = ST->element_end(); stb != ste; ++stb) {
      if (!containsPointerType(*stb)) {
        continue;
      }
      return true;
    }
    break;
  }
  case Type::VectorTyID:
  case Type::ArrayTyID:
    return containsPointerType(dyn_cast<SequentialType>(Ty)->getElementType());
  case Type::PointerTyID:
    return true;
  };
  return false;
}

bool 
AMDILPeepholeOpt::dumpAllIntoArena(Function &F) 
{
  bool dumpAll = false;
  for (Function::const_arg_iterator cab = F.arg_begin(),
       cae = F.arg_end(); cab != cae; ++cab) {
    const Argument *arg = cab;
    const PointerType *PT = dyn_cast<PointerType>(arg->getType());
    if (!PT) {
      continue;
    }
    Type *DereferencedType = PT->getElementType();
    if (!dyn_cast<StructType>(DereferencedType) 
#if LLVM_VERSION < 2500
        && !dyn_cast<UnionType>(DereferencedType)
#endif
        ) {
      continue;
    }
    if (!containsPointerType(DereferencedType)) {
      continue;
    }
    // FIXME: Because a pointer inside of a struct/union may be aliased to
    // another pointer we need to take the conservative approach and place all
    // pointers into the arena until more advanced detection is implemented.
    dumpAll = true;
  }
  return dumpAll;
}
void
AMDILPeepholeOpt::doIsConstCallConversionIfNeeded()
{
  if (isConstVec.empty()) {
    return;
  }
  for (unsigned x = 0, y = isConstVec.size(); x < y; ++x) {
    CallInst *CI = isConstVec[x];
#if LLVM_VERSION >= 2500
    Constant *CV = dyn_cast<Constant>(CI->getOperand(0));
#else
    Constant *CV = dyn_cast<Constant>(CI->getOperand(1));
#endif
    Type *aType = Type::getInt32Ty(*mCTX);
    Value *Val = (CV != NULL) ? ConstantInt::get(aType, 1)
      : ConstantInt::get(aType, 0);
    CI->replaceAllUsesWith(Val);
    CI->eraseFromParent();
  }
  isConstVec.clear();
}
void 
AMDILPeepholeOpt::doAtomicConversionIfNeeded(Function &F) 
{
  // Don't do anything if we don't have any atomic operations.
  if (atomicFuncs.empty()) {
    return;
  }
  // Change the function name for the atomic if it is required
  uint32_t size = atomicFuncs.size();
  for (uint32_t x = 0; x < size; ++x) {
#if LLVM_VERSION >= 2500
    atomicFuncs[x].first->setOperand(
        atomicFuncs[x].first->getNumOperands()-1, 
        atomicFuncs[x].second);
#else
    atomicFuncs[x].first->setOperand(0, atomicFuncs[x].second);
#endif

  }
  mChanged = true;
  if (mConvertAtomics) {
    return;
  }
  // If we did not convert all of the atomics, then we need to make sure that
  // the atomics that were not converted have their base pointers set to use the
  // arena path.
  Function::arg_iterator argB = F.arg_begin();
  Function::arg_iterator argE = F.arg_end();
  AMDILKernelManager *KM = (AMDILKernelManager *) mSTM->getKernelManager();
  AMDILMachineFunctionInfo *mMFI = getAnalysis<MachineFunctionAnalysis>().getMF()
    .getInfo<AMDILMachineFunctionInfo>();
  for (; argB != argE; ++argB) {
    if (mSTM->device()->isSupported(AMDILDeviceInfo::ArenaUAV)) {
      KM->setUAVID(argB,mSTM->device()->getResourceID(AMDILDevice::ARENA_UAV_ID));
      mMFI->uav_insert(mSTM->device()->getResourceID(AMDILDevice::ARENA_UAV_ID));
    } else {
      KM->setUAVID(argB,mSTM->device()->getResourceID(AMDILDevice::GLOBAL_ID));
      mMFI->uav_insert(mSTM->device()->getResourceID(AMDILDevice::GLOBAL_ID));
    }
  }
}

bool 
AMDILPeepholeOpt::runOnFunction(Function &MF) 
{
  mChanged = false;
  mF = &MF;
  mSTM = &TM.getSubtarget<AMDILSubtarget>();
  if (mDebug) {
    MF.dump();
  }
  mCTX = &MF.getType()->getContext();
  mConvertAtomics = true;
  if (dumpAllIntoArena(MF)) {
    for (Function::const_arg_iterator cab = MF.arg_begin(),
         cae = MF.arg_end(); cab != cae; ++cab) {
      const Argument *arg = cab;
      AMDILKernelManager *KM = (AMDILKernelManager *)mSTM->getKernelManager();
      KM->setUAVID(getBasePointerValue(arg),
          mSTM->device()->getResourceID(AMDILDevice::GLOBAL_ID));
    }
  }
  mRWGOpt = mSTM->getGlobalManager()->hasRWG(MF.getName());
  safeNestedForEach(MF.begin(), MF.end(), MF.begin()->begin(),
     std::bind1st(std::mem_fun(&AMDILPeepholeOpt::instLevelOptimizations),
                  this));

  doAtomicConversionIfNeeded(MF);
  doIsConstCallConversionIfNeeded();

  if (mDebug) {
    MF.dump();
  }
  return mChanged;
}
#if 0
// This currently isn't used, but might be used in the future, 
// so not removing it. Just commenting it out to fix linux warnings.
static Value* getSignExtend24(Value *op, Instruction *oldOp)
{
  Constant *s8val;
  Type *opType = op->getType();

static Value *getSignExtend24(Value *op, Instruction *oldOp) {
  Constant *s8val;
  Type *opType = op->getType();
  if (opType->isVectorTy()) {
    std::vector<Constant *> consts;
    for (size_t x = 0, y = dyn_cast<VectorType>(opType)->getNumElements();
         x < y; ++x) {
      consts.push_back(ConstantInt::get(Type::getInt32Ty(opType->getContext()),
                                        8));
    }
#if LLVM_VERSION >= 3212
    s8val = ConstantVector::get(consts);
#else
    s8val = ConstantVector::get(dyn_cast<VectorType>(opType), consts);
#endif
  } else {
    s8val = ConstantInt::get(Type::getInt32Ty(opType->getContext()), 8);
  }
  BinaryOperator *res = BinaryOperator::Create(Instruction::Shl,
                                               op, s8val, "bit24shl", oldOp);
  res = BinaryOperator::Create(Instruction::AShr, res, s8val,
                               "bit24shr", oldOp);
  return res;
}
#endif

bool 
AMDILPeepholeOpt::optimizeCallInst(BasicBlock::iterator *bbb) 
{
  Instruction *inst = (*bbb);
  CallInst *CI = dyn_cast<CallInst>(inst);
  if (!CI) {
    return false;
  }
  if (isSigned24BitOps(CI)) {
    expandSigned24BitOps(CI);
    ++(*bbb);
    CI->eraseFromParent();
    return true;
  }
  if (isRWGLocalOpt(CI)) {
    expandRWGLocalOpt(CI);
    return false;
  }
  if (propagateSamplerInst(CI)) {
    return false;
  }
  if (expandBFI(CI) || expandBFM(CI)) {
    ++(*bbb);
    CI->eraseFromParent();
    return true;
  }
  if (convertAccurateDivide(CI)) {
    expandAccurateDivide(CI);
    ++(*bbb);
    CI->eraseFromParent();
    return true;
  }

#if LLVM_VERSION >= 2500
  StringRef calleeName = CI->getOperand(CI->getNumOperands()-1)->getName();
#else
  StringRef calleeName = CI->getOperand(0)->getName();
#endif
  if (calleeName.startswith("__amdil_is_constant")) {
    // If we do not have optimizations, then this
    // cannot be properly evaluated, so we add the
    // call instruction to a vector and process
    // them at the end of processing after the
    // samplers have been correctly handled.
    if (optLevel == CodeGenOpt::None) {
      isConstVec.push_back(CI);
      return false;
    } else {
#if LLVM_VERSION >= 2500
      Constant *CV = dyn_cast<Constant>(CI->getOperand(0));
#else
      Constant *CV = dyn_cast<Constant>(CI->getOperand(1));
#endif
      Type *aType = Type::getInt32Ty(*mCTX);
      Value *Val = (CV != NULL) ? ConstantInt::get(aType, 1)
        : ConstantInt::get(aType, 0);
      CI->replaceAllUsesWith(Val);
      ++(*bbb);
      CI->eraseFromParent();
      return true;
    }
  }

  if (calleeName.equals("__amdil_is_asic_id_i32")) {
#if LLVM_VERSION >= 2500
    ConstantInt *CV = dyn_cast<ConstantInt>(CI->getOperand(0));
#else
    ConstantInt *CV = dyn_cast<ConstantInt>(CI->getOperand(1));
#endif
    Type *aType = Type::getInt32Ty(*mCTX);
    Value *Val = CV;
    if (Val) {
      Val = ConstantInt::get(aType, 
          mSTM->device()->getDeviceFlag() & CV->getZExtValue());
    } else {
      Val = ConstantInt::get(aType, 0);
    }
    CI->replaceAllUsesWith(Val);
    ++(*bbb);
    CI->eraseFromParent();
    return true;
  }
#if LLVM_VERSION >= 2500
  Function *F = dyn_cast<Function>(CI->getOperand(CI->getNumOperands()-1));
#else
  Function *F = dyn_cast<Function>(CI->getOperand(0));
#endif
  if (!F) {
    return false;
  } 
  if (F->getName().startswith("__atom") && !CI->getNumUses() 
      && F->getName().find("_xchg") == StringRef::npos) {
    std::string buffer(F->getName().str() + "_noret");
    F = dyn_cast<Function>(
          F->getParent()->getOrInsertFunction(buffer, F->getFunctionType()));
    atomicFuncs.push_back(std::make_pair <CallInst*, Function*>(CI, F));
  }
  
  if (!mSTM->device()->isSupported(AMDILDeviceInfo::ArenaSegment)
      && !mSTM->device()->isSupported(AMDILDeviceInfo::MultiUAV)) {
    return false;
  }
  if (!mConvertAtomics) {
    return false;
  }
  StringRef name = F->getName();
  if (name.startswith("__atom") && name.find("_g") != StringRef::npos) {
#if LLVM_VERSION >= 2500
    Value *ptr = CI->getOperand(0);
#else
    Value *ptr = CI->getOperand(1);
#endif
    const Value *basePtr = getBasePointerValue(ptr);
    const Argument *Arg = dyn_cast<Argument>(basePtr);
    if (Arg) {
      AMDILGlobalManager *GM = (AMDILGlobalManager*)mSTM->getGlobalManager();
      int32_t id = GM->getArgID(Arg);
      if (id >= 0) {
        std::stringstream ss;
        ss << name.data() << "_" << id << '\n';
        std::string val;
        ss >> val;
        F = dyn_cast<Function>(
              F->getParent() ->getOrInsertFunction(val, F->getFunctionType()));
        atomicFuncs.push_back(std::make_pair <CallInst*, Function*>(CI, F));
      } else {
        mConvertAtomics = false;
      }
    } else {
      mConvertAtomics = false;
    }
  }
  return false;
}

bool
AMDILPeepholeOpt::setupBitInsert(Instruction *base, 
    Instruction *&src, 
    Constant *&mask, 
    Constant *&shift)
{
  if (!base) {
    if (mDebug) {
      dbgs() << "Null pointer passed into function.\n";
    }
    return false;
  }
  bool andOp = false;
  if (base->getOpcode() == Instruction::Shl) {
    shift = dyn_cast<Constant>(base->getOperand(1));
  } else if (base->getOpcode() == Instruction::And) {
    mask = dyn_cast<Constant>(base->getOperand(1));
    andOp = true;
  } else {
    if (mDebug) {
      dbgs() << "Failed setup with no Shl or And instruction on base opcode!\n";
    }
    // If the base is neither a Shl or a And, we don't fit any of the patterns above.
    return false;
  }
  src = dyn_cast<Instruction>(base->getOperand(0));
  if (!src) {
    if (mDebug) {
      dbgs() << "Failed setup since the base operand is not an instruction!\n";
    }
    return false;
  }
  // If we find an 'and' operation, then we don't need to
  // find the next operation as we already know the
  // bits that are valid at this point.
  if (andOp) {
    return true;
  }
  if (src->getOpcode() == Instruction::Shl && !shift) {
    shift = dyn_cast<Constant>(src->getOperand(1));
    src = dyn_cast<Instruction>(src->getOperand(0));
  } else if (src->getOpcode() == Instruction::And && !mask) {
    mask = dyn_cast<Constant>(src->getOperand(1));
  }
  if (!mask && !shift) {
    if (mDebug) {
      dbgs() << "Failed setup since both mask and shift are NULL!\n";
    }
    // Did not find a constant mask or a shift.
    return false;
  }
  return true;
}
bool
AMDILPeepholeOpt::optimizeBitInsert(Instruction *inst) 
{
  if (!inst) {
    return false;
  }
  if (!inst->isBinaryOp()) {
    return false;
  }
  if (inst->getOpcode() != Instruction::Or) {
    return false;
  }
  if (optLevel == CodeGenOpt::None) {
    return false;
  }
  // We want to do an optimization on a sequence of ops that in the end equals a
  // single ISA instruction.
  // The base pattern for this optimization is - ((A & B) << C) | ((D & E) << F)
  // Some simplified versions of this pattern are as follows:
  // (A & B) | (D & E) when B & E == 0 && C == 0 && F == 0
  // ((A & B) << C) | (D & E) when B ^ E == 0 && (1 << C) >= E
  // (A & B) | ((D & E) << F) when B ^ E == 0 && (1 << F) >= B
  // (A & B) | (D << F) when (1 << F) >= B
  // (A << C) | (D & E) when (1 << C) >= E
  if (mSTM->device()->getGeneration() == AMDILDeviceInfo::HD4XXX) {
    // The HD4XXX hardware doesn't support the ubit_insert instruction.
    return false;
  }
  Type *aType = inst->getType();
  bool isVector = aType->isVectorTy();
  int numEle = 1;
  // This optimization only works on 32bit integers.
  if (aType->getScalarType()
      != Type::getInt32Ty(inst->getContext())) {
    return false;
  }
  if (isVector) {
    const VectorType *VT = dyn_cast<VectorType>(aType);
    numEle = VT->getNumElements();
    // We currently cannot support more than 4 elements in a intrinsic and we
    // cannot support Vec3 types.
    if (numEle > 4 || numEle == 3) {
      return false;
    }
  }
  // TODO: Handle vectors.
  if (isVector) {
    if (mDebug) {
      dbgs() << "!!! Vectors are not supported yet!\n";
    }
    return false;
  }
  Instruction *LHSSrc = NULL, *RHSSrc = NULL;
  Constant *LHSMask = NULL, *RHSMask = NULL;
  Constant *LHSShift = NULL, *RHSShift = NULL;
  Instruction *LHS = dyn_cast<Instruction>(inst->getOperand(0));
  Instruction *RHS = dyn_cast<Instruction>(inst->getOperand(1));
  if (!setupBitInsert(LHS, LHSSrc, LHSMask, LHSShift)) {
    if (mDebug) {
      dbgs() << "Found an OR Operation that failed setup!\n";
      inst->dump();
      if (LHS) { LHS->dump(); }
      if (LHSSrc) { LHSSrc->dump(); }
      if (LHSMask) { LHSMask->dump(); }
      if (LHSShift) { LHSShift->dump(); }
    }
    // There was an issue with the setup for BitInsert.
    return false;
  }
  if (!setupBitInsert(RHS, RHSSrc, RHSMask, RHSShift)) {
    if (mDebug) {
      dbgs() << "Found an OR Operation that failed setup!\n";
      inst->dump();
      if (RHS) { RHS->dump(); }
      if (RHSSrc) { RHSSrc->dump(); }
      if (RHSMask) { RHSMask->dump(); }
      if (RHSShift) { RHSShift->dump(); }
    }
    // There was an issue with the setup for BitInsert.
    return false;
  }
  if (mDebug) {
    dbgs() << "Found an OR operation that can possible be optimized to ubit insert!\n";
    dbgs() << "Op:        "; inst->dump();
    dbgs() << "LHS:       "; if (LHS) { LHS->dump(); } else { dbgs() << "(None)\n"; }
    dbgs() << "LHS Src:   "; if (LHSSrc) { LHSSrc->dump(); } else { dbgs() << "(None)\n"; }
    dbgs() << "LHS Mask:  "; if (LHSMask) { LHSMask->dump(); } else { dbgs() << "(None)\n"; }
    dbgs() << "LHS Shift: "; if (LHSShift) { LHSShift->dump(); } else { dbgs() << "(None)\n"; }
    dbgs() << "RHS:       "; if (RHS) { RHS->dump(); } else { dbgs() << "(None)\n"; }
    dbgs() << "RHS Src:   "; if (RHSSrc) { RHSSrc->dump(); } else { dbgs() << "(None)\n"; }
    dbgs() << "RHS Mask:  "; if (RHSMask) { RHSMask->dump(); } else { dbgs() << "(None)\n"; }
    dbgs() << "RHS Shift: "; if (RHSShift) { RHSShift->dump(); } else { dbgs() << "(None)\n"; }
  }
  Constant *offset = NULL;
  Constant *width = NULL;
  int32_t lhsMaskVal = 0, rhsMaskVal = 0;
  int32_t lhsShiftVal = 0, rhsShiftVal = 0;
  int32_t lhsMaskWidth = 0, rhsMaskWidth = 0;
  int32_t lhsMaskOffset = 0, rhsMaskOffset = 0;
  lhsMaskVal = (int32_t)(LHSMask 
      ? dyn_cast<ConstantInt>(LHSMask)->getZExtValue() : 0);
  rhsMaskVal = (int32_t)(RHSMask 
      ? dyn_cast<ConstantInt>(RHSMask)->getZExtValue() : 0);
  lhsShiftVal = (int32_t)(LHSShift 
      ? dyn_cast<ConstantInt>(LHSShift)->getZExtValue() : 0);
  rhsShiftVal = (int32_t)(RHSShift 
      ? dyn_cast<ConstantInt>(RHSShift)->getZExtValue() : 0);
  lhsMaskWidth = lhsMaskVal ? CountPopulation_32(lhsMaskVal) : 32 - lhsShiftVal;
  rhsMaskWidth = rhsMaskVal ? CountPopulation_32(rhsMaskVal) : 32 - rhsShiftVal;
  lhsMaskOffset = lhsMaskVal ? CountTrailingZeros_32(lhsMaskVal) : lhsShiftVal;
  rhsMaskOffset = rhsMaskVal ? CountTrailingZeros_32(rhsMaskVal) : rhsShiftVal;
  // TODO: Handle the case of A & B | D & ~B(i.e. inverted masks).
  if (mDebug) {
      dbgs() << "Found pattern: \'((A" << (LHSMask ? " & B)" : ")");
      dbgs() << (LHSShift ? " << C)" : ")") << " | ((D" ;
      dbgs() << (RHSMask ? " & E)" : ")");
      dbgs() << (RHSShift ? " << F)\'\n" : ")\'\n");
      dbgs() << "A = LHSSrc\t\tD = RHSSrc \n";
      dbgs() << "B = " << lhsMaskVal << "\t\tE = " << rhsMaskVal << "\n";
      dbgs() << "C = " << lhsShiftVal << "\t\tF = " << rhsShiftVal << "\n";
      dbgs() << "width(B) = " << lhsMaskWidth;
      dbgs() << "\twidth(E) = " << rhsMaskWidth << "\n";
      dbgs() << "offset(B) = " << lhsMaskOffset;
      dbgs() << "\toffset(E) = " << rhsMaskOffset << "\n";
      dbgs() << "Constraints: \n";
      dbgs() << "\t(1) B ^ E == 0\n";
      dbgs() << "\t(2-LHS) B is a mask\n";
      dbgs() << "\t(2-LHS) E is a mask\n";
      dbgs() << "\t(3-LHS) (offset(B)) >= (width(E) + offset(E))\n";
      dbgs() << "\t(3-RHS) (offset(E)) >= (width(B) + offset(B))\n";
  }
  if ((lhsMaskVal || rhsMaskVal) && !(lhsMaskVal ^ rhsMaskVal)) {
    if (mDebug) {
      dbgs() << lhsMaskVal << " ^ " << rhsMaskVal;
      dbgs() << " = " << (lhsMaskVal ^ rhsMaskVal) << "\n";
      dbgs() << "Failed constraint 1!\n";
    }
    return false;
  }
  if (mDebug) {
    dbgs() << "LHS = " << lhsMaskOffset << "";
    dbgs() << " >= (" << rhsMaskWidth << " + " << rhsMaskOffset << ") = ";
    dbgs() << (lhsMaskOffset >= (rhsMaskWidth + rhsMaskOffset));
    dbgs() << "\nRHS = " << rhsMaskOffset << "";
    dbgs() << " >= (" << lhsMaskWidth << " + " << lhsMaskOffset << ") = ";
    dbgs() << (rhsMaskOffset >= (lhsMaskWidth + lhsMaskOffset));
    dbgs() << "\n";
  }
  if (lhsMaskOffset >= (rhsMaskWidth + rhsMaskOffset)) {
    offset = ConstantInt::get(aType, lhsMaskOffset, false);
    width = ConstantInt::get(aType, lhsMaskWidth, false);
    RHSSrc = RHS;
    if (!isMask_32(lhsMaskVal) && !isShiftedMask_32(lhsMaskVal)) {
      if (mDebug) {
        dbgs() << "Value is not a Mask: " << lhsMaskVal << "\n";
        dbgs() << "Failed constraint 2!\n";
      }
      return false;
    }
    if (!LHSShift) {
      LHSSrc = BinaryOperator::Create(Instruction::LShr, LHSSrc, offset,
          "MaskShr", LHS);
    } else if (lhsShiftVal != lhsMaskOffset) {
      LHSSrc = BinaryOperator::Create(Instruction::LShr, LHSSrc, offset,
          "MaskShr", LHS);
    }
    if (mDebug) {
      dbgs() << "Optimizing LHS!\n";
    }
  } else if (rhsMaskOffset >= (lhsMaskWidth + lhsMaskOffset)) {
    offset = ConstantInt::get(aType, rhsMaskOffset, false);
    width = ConstantInt::get(aType, rhsMaskWidth, false);
    LHSSrc = RHSSrc;
    RHSSrc = LHS;
    if (!isMask_32(rhsMaskVal) && !isShiftedMask_32(rhsMaskVal)) {
      if (mDebug) {
        dbgs() << "Non-Mask: " << rhsMaskVal << "\n";
        dbgs() << "Failed constraint 2!\n";
      }
      return false;
    }
    if (!RHSShift) {
      LHSSrc = BinaryOperator::Create(Instruction::LShr, LHSSrc, offset,
          "MaskShr", RHS);
    } else if (rhsShiftVal != rhsMaskOffset) {
      LHSSrc = BinaryOperator::Create(Instruction::LShr, LHSSrc, offset,
          "MaskShr", RHS);
    }
    if (mDebug) {
      dbgs() << "Optimizing RHS!\n";
    }
  } else {
    if (mDebug) {
      dbgs() << "Failed constraint 3!\n";
    }
    return false;
  }
  if (mDebug) {
    dbgs() << "Width:  "; if (width) { width->dump(); } else { dbgs() << "(0)\n"; }
    dbgs() << "Offset: "; if (offset) { offset->dump(); } else { dbgs() << "(0)\n"; }
    dbgs() << "LHSSrc: "; if (LHSSrc) { LHSSrc->dump(); } else { dbgs() << "(0)\n"; }
    dbgs() << "RHSSrc: "; if (RHSSrc) { RHSSrc->dump(); } else { dbgs() << "(0)\n"; }
  }
  if (!offset || !width) {
    if (mDebug) {
      dbgs() << "Either width or offset are NULL, failed detection!\n";
    }
    return false;
  }
  // Lets create the function signature.
  std::vector<Type *> callTypes;
  callTypes.push_back(aType);
  callTypes.push_back(aType);
  callTypes.push_back(aType);
  callTypes.push_back(aType);
  FunctionType *funcType = FunctionType::get(aType, callTypes, false);
  std::string name = "__amdil_ubit_insert";
  if (isVector) { name += "_v" + itostr(numEle) + "u32"; } else { name += "_u32"; }
  Function *Func = 
    dyn_cast<Function>(inst->getParent()->getParent()->getParent()->
        getOrInsertFunction(llvm::StringRef(name), funcType));
  Value *Operands[4] = {
    width,
    offset,
    LHSSrc,
    RHSSrc
  };
  CallInst *CI = CallInst::Create(Func, Operands, "BitInsertOpt");
  if (mDebug) {
    dbgs() << "Old Inst: ";
    inst->dump();
    dbgs() << "New Inst: ";
    CI->dump();
    dbgs() << "\n\n";
  }
  CI->insertBefore(inst);
  inst->replaceAllUsesWith(CI);
  return true;
}

bool 
AMDILPeepholeOpt::optimizeBitExtract(Instruction *inst) 
{
  if (!inst) {
    return false;
  }
  if (!inst->isBinaryOp()) {
    return false;
  }
  if (inst->getOpcode() != Instruction::And) {
    return false;
  }
  if (optLevel == CodeGenOpt::None) {
    return false;
  }
  // We want to do some simple optimizations on Shift right/And patterns. The
  // basic optimization is to turn (A >> B) & C where A is a 32bit type, B is a
  // value smaller than 32 and C is a mask. If C is a constant value, then the
  // following transformation can occur. For signed integers, it turns into the
  // function call dst = __amdil_ibit_extract(log2(C), B, A) For unsigned
  // integers, it turns into the function call dst =
  // __amdil_ubit_extract(log2(C), B, A) The function __amdil_[u|i]bit_extract
  // can be found in Section 7.9 of the ATI IL spec of the stream SDK for
  // Evergreen hardware.
  if (mSTM->device()->getGeneration() == AMDILDeviceInfo::HD4XXX) {
    // This does not work on HD4XXX hardware.
    return false;
  }
  Type *aType = inst->getType();
  bool isVector = aType->isVectorTy();
  int numEle = 1;
  // This only works on 32bit integers
  if (aType->getScalarType()
      != Type::getInt32Ty(inst->getContext())) {
    return false;
  }
  if (isVector) {
    const VectorType *VT = dyn_cast<VectorType>(aType);
    numEle = VT->getNumElements();
    // We currently cannot support more than 4 elements in a intrinsic and we
    // cannot support Vec3 types.
    if (numEle > 4 || numEle == 3) {
      return false;
    }
  }
  BinaryOperator *ShiftInst = dyn_cast<BinaryOperator>(inst->getOperand(0));
  // If the first operand is not a shift instruction, then we can return as it
  // doesn't match this pattern.
  if (!ShiftInst || !ShiftInst->isShift()) {
    return false;
  }
  // If we are a shift left, then we need don't match this pattern.
  if (ShiftInst->getOpcode() == Instruction::Shl) {
    return false;
  }
  bool isSigned = ShiftInst->isArithmeticShift();
  Constant *AndMask = dyn_cast<Constant>(inst->getOperand(1));
  Constant *ShrVal = dyn_cast<Constant>(ShiftInst->getOperand(1));
  // Lets make sure that the shift value and the and mask are constant integers.
  if (!AndMask || !ShrVal) {
    return false;
  }
  Constant *newMaskConst;
  Constant *shiftValConst;
  if (isVector) {
    // Handle the vector case
    std::vector<Constant *> maskVals;
    std::vector<Constant *> shiftVals;
    ConstantVector *AndMaskVec = dyn_cast<ConstantVector>(AndMask);
    ConstantVector *ShrValVec = dyn_cast<ConstantVector>(ShrVal);
    Type *scalarType = AndMaskVec->getType()->getScalarType();
    assert(AndMaskVec->getNumOperands() ==
           ShrValVec->getNumOperands() && "cannot have a "
           "combination where the number of elements to a "
           "shift and an and are different!");
    for (size_t x = 0, y = AndMaskVec->getNumOperands(); x < y; ++x) {
      ConstantInt *AndCI = dyn_cast<ConstantInt>(AndMaskVec->getOperand(x));
      ConstantInt *ShiftIC = dyn_cast<ConstantInt>(ShrValVec->getOperand(x));
      if (!AndCI || !ShiftIC) {
        return false;
      }
      uint32_t maskVal = (uint32_t)AndCI->getZExtValue();
      if (!isMask_32(maskVal)) {
        return false;
      }
      maskVal = (uint32_t)CountTrailingOnes_32(maskVal);
      uint32_t shiftVal = (uint32_t)ShiftIC->getZExtValue();
      // If the mask or shiftval is greater than the bitcount, then break out.
      if (maskVal >= 32 || shiftVal >= 32) {
        return false;
      }
      // If the mask val is greater than the the number of original bits left
      // then this optimization is invalid.
      if (maskVal > (32 - shiftVal)) {
        return false;
      }
      maskVals.push_back(ConstantInt::get(scalarType, maskVal, isSigned));
      shiftVals.push_back(ConstantInt::get(scalarType, shiftVal, isSigned));
    }
    newMaskConst = ConstantVector::get(maskVals);
    shiftValConst = ConstantVector::get(shiftVals);
  } else {
    // Handle the scalar case
    uint32_t maskVal = (uint32_t)dyn_cast<ConstantInt>(AndMask)->getZExtValue();
    // This must be a mask value where all lower bits are set to 1 and then any
    // bit higher is set to 0.
    if (!isMask_32(maskVal)) {
      return false;
    }
    maskVal = (uint32_t)CountTrailingOnes_32(maskVal);
    // Count the number of bits set in the mask, this is the width of the
    // resulting bit set that is extracted from the source value.
    uint32_t shiftVal = (uint32_t)dyn_cast<ConstantInt>(ShrVal)->getZExtValue();
    // If the mask or shift val is greater than the bitcount, then break out.
    if (maskVal >= 32 || shiftVal >= 32) {
      return false;
    }
    // If the mask val is greater than the the number of original bits left then
    // this optimization is invalid.
    if (maskVal > (32 - shiftVal)) {
      return false;
    }
    newMaskConst = ConstantInt::get(aType, maskVal, isSigned);
    shiftValConst = ConstantInt::get(aType, shiftVal, isSigned);
  }
  // Lets create the function signature.
  std::vector<Type *> callTypes;
  callTypes.push_back(aType);
  callTypes.push_back(aType);
  callTypes.push_back(aType);
  FunctionType *funcType = FunctionType::get(aType, callTypes, false);
  std::string name = "__amdil_ubit_extract";
  if (isVector) {
    name += "_v" + itostr(numEle) + "i32";
  } else {
    name += "_i32";
  }
  // Lets create the function.
  Function *Func = 
    dyn_cast<Function>(inst->getParent()->getParent()->getParent()->
                       getOrInsertFunction(llvm::StringRef(name), funcType));
  Value *Operands[3] = {
    newMaskConst,
    shiftValConst,
    ShiftInst->getOperand(0)
  };
  // Lets create the Call with the operands
  CallInst *CI = CallInst::Create(Func, Operands, "ByteExtractOpt");
  CI->insertBefore(inst);
  inst->replaceAllUsesWith(CI);
  return true;
}

bool
AMDILPeepholeOpt::expandBFI(CallInst *CI)
{
  if (!CI || mSTM->calVersion() <= CAL_VERSION_SC_150) {
    return false;
  }
#if LLVM_VERSION >= 2500
  Value *LHS = CI->getOperand(CI->getNumOperands() - 1);
#else
  Value *LHS = CI->getOperand(0);
#endif
  if (!LHS->getName().startswith("__amdil_bfi")) {
    return false;
  }
  Type* type = CI->getOperand(0)->getType();
  Constant *negOneConst = NULL;
  if (type->isVectorTy()) {
    std::vector<Constant *> negOneVals;
    negOneConst = ConstantInt::get(CI->getContext(), 
        APInt(32, StringRef("-1"), 10));
    for (size_t x = 0,
        y = dyn_cast<VectorType>(type)->getNumElements(); x < y; ++x) {
      negOneVals.push_back(negOneConst);
    }
    negOneConst = ConstantVector::get(negOneVals);
  } else {
    negOneConst = ConstantInt::get(CI->getContext(), 
        APInt(32, StringRef("-1"), 10));
  }
  // __amdil_bfi => (A & B) | (~A & C)
  BinaryOperator *lhs = 
    BinaryOperator::Create(Instruction::And, CI->getOperand(0),
        CI->getOperand(1), "bfi_and", CI);
  BinaryOperator *rhs =
    BinaryOperator::Create(Instruction::Xor, CI->getOperand(0), negOneConst,
        "bfi_not", CI);
  rhs = BinaryOperator::Create(Instruction::And, rhs, CI->getOperand(2),
      "bfi_and", CI);
  lhs = BinaryOperator::Create(Instruction::Or, lhs, rhs, "bfi_or", CI);
  CI->replaceAllUsesWith(lhs);
  return true;
}

bool
AMDILPeepholeOpt::expandBFM(CallInst *CI)
{
  if (!CI || mSTM->calVersion() <= CAL_VERSION_SC_150) {
    return false;
  }
#if LLVM_VERSION >= 2500
  Value *LHS = CI->getOperand(CI->getNumOperands() - 1);
#else
  Value *LHS = CI->getOperand(0);
#endif
  if (!LHS->getName().startswith("__amdil_bfm")) {
    return false;
  }
  // __amdil_bfm => ((1 << (src0 & 0x1F)) - 1) << (src1 & 0x1f)
  Constant *newMaskConst = NULL;
  Constant *newShiftConst = NULL;
  Type* type = CI->getOperand(0)->getType();
  if (type->isVectorTy()) {
    std::vector<Constant*> newMaskVals, newShiftVals;
    newMaskConst = ConstantInt::get(Type::getInt32Ty(*mCTX), 0x1F);
    newShiftConst = ConstantInt::get(Type::getInt32Ty(*mCTX), 1);
    for (size_t x = 0,
        y = dyn_cast<VectorType>(type)->getNumElements(); x < y; ++x) {
      newMaskVals.push_back(newMaskConst);
      newShiftVals.push_back(newShiftConst);
    }
    newMaskConst = ConstantVector::get(newMaskVals);
    newShiftConst = ConstantVector::get(newShiftVals);
  } else {
    newMaskConst = ConstantInt::get(Type::getInt32Ty(*mCTX), 0x1F);
    newShiftConst = ConstantInt::get(Type::getInt32Ty(*mCTX), 1);
  }
  BinaryOperator *lhs =
    BinaryOperator::Create(Instruction::And, CI->getOperand(0),
        newMaskConst, "bfm_mask", CI);
  lhs = BinaryOperator::Create(Instruction::Shl, newShiftConst,
      lhs, "bfm_shl", CI);
  lhs = BinaryOperator::Create(Instruction::Sub, lhs,
      newShiftConst, "bfm_sub", CI);
  BinaryOperator *rhs =
    BinaryOperator::Create(Instruction::And, CI->getOperand(1),
        newMaskConst, "bfm_mask", CI);
  lhs = BinaryOperator::Create(Instruction::Shl, lhs, rhs, "bfm_shl", CI);
  CI->replaceAllUsesWith(lhs);
  return true;
}

bool
AMDILPeepholeOpt::instLevelOptimizations(BasicBlock::iterator *bbb) 
{
  Instruction *inst = (*bbb);
  if (optimizeCallInst(bbb)) {
    return true;
  }
  if (optimizeBitExtract(inst)) {
    return false;
  }
  if (optimizeBitInsert(inst)) {
    return false;
  }
  if (correctMisalignedMemOp(inst)) {
    return false;
  }
  return false;
}
bool
AMDILPeepholeOpt::correctMisalignedMemOp(Instruction *inst)
{
  LoadInst *linst = dyn_cast<LoadInst>(inst);
  StoreInst *sinst = dyn_cast<StoreInst>(inst);
  unsigned alignment;
  Type* Ty = inst->getType();
  if (linst) {
    alignment = linst->getAlignment();
    Ty = inst->getType();
  } else if (sinst) {
    alignment = sinst->getAlignment();
    Ty = sinst->getValueOperand()->getType();
  } else {
    return false;
  }
  unsigned size = getTypeSize(Ty);
  if (size == alignment || size < alignment) {
    return false;
  }
#if LLVM_VERSION >= 2500
  if (!Ty->isStructTy()) {
#else
  if (!Ty->isStructTy() && !Ty->isUnionTy()) {
#endif
    return false;
  }
  if (alignment < 4) {
    if (linst) {
      linst->setAlignment(0);
      return true;
    } else if (sinst) {
      sinst->setAlignment(0);
      return true;
    }
  }
  return false;
}
bool 
AMDILPeepholeOpt::isSigned24BitOps(CallInst *CI) 
{
  if (!CI) {
    return false;
  }
#if LLVM_VERSION >= 2500
  Value *LHS = CI->getOperand(CI->getNumOperands() - 1);
#else
  Value *LHS = CI->getOperand(0);
#endif
  std::string namePrefix = LHS->getName().substr(0, 14);
  if (namePrefix != "__amdil_imad24" && namePrefix != "__amdil_imul24"
      && namePrefix != "__amdil__imul24_high") {
    return false;
  }
  if (mSTM->device()->usesHardware(AMDILDeviceInfo::Signed24BitOps)) {
    return false;
  }
  return true;
}

void 
AMDILPeepholeOpt::expandSigned24BitOps(CallInst *CI) 
{
  assert(isSigned24BitOps(CI) && "Must be a "
      "signed 24 bit operation to call this function!");
#if LLVM_VERSION >= 2500
  Value *LHS = CI->getOperand(CI->getNumOperands()-1);
#else
  Value *LHS = CI->getOperand(0);
#endif
  // On 7XX and 8XX we do not have signed 24bit, so we need to
  // expand it to the following:
  // imul24 turns into 32bit imul
  // imad24 turns into 32bit imad
  // imul24_high turns into 32bit imulhigh
  if (LHS->getName().substr(0, 14) == "__amdil_imad24") {
#if LLVM_VERSION >= 2500
    Type *aType = CI->getOperand(0)->getType();
    bool isVector = aType->isVectorTy();
    int numEle = isVector ? dyn_cast<VectorType>(aType)->getNumElements() : 1;
    std::vector<Type*> callTypes;
    callTypes.push_back(CI->getOperand(0)->getType());
    callTypes.push_back(CI->getOperand(1)->getType());
    callTypes.push_back(CI->getOperand(2)->getType());
    FunctionType *funcType =
      FunctionType::get(CI->getOperand(0)->getType(), callTypes, false);
#else
    Type *aType = CI->getOperand(1)->getType();
    bool isVector = aType->isVectorTy();
    int numEle = isVector ? dyn_cast<VectorType>(aType)->getNumElements() : 1;
    std::vector<Type*> callTypes;
    callTypes.push_back(CI->getOperand(1)->getType());
    callTypes.push_back(CI->getOperand(2)->getType());
    callTypes.push_back(CI->getOperand(3)->getType());
    FunctionType *funcType =
      FunctionType::get(CI->getOperand(1)->getType(), callTypes, false);
#endif
    std::string name = "__amdil_imad";
    if (isVector) {
      name += "_v" + itostr(numEle) + "i32";
    } else {
      name += "_i32";
    }
    Function *Func = dyn_cast<Function>(
                       CI->getParent()->getParent()->getParent()->
                       getOrInsertFunction(llvm::StringRef(name), funcType));
#if LLVM_VERSION >= 2500
    Value *Operands[3] = {
      CI->getOperand(0),
      CI->getOperand(1),
      CI->getOperand(2)
    };
#else
    Value *Operands[3] = {
      CI->getOperand(1),
      CI->getOperand(2),
      CI->getOperand(3)
    };
#endif
    CallInst *nCI = CallInst::Create(Func, Operands, "imad24");
    nCI->insertBefore(CI);
    CI->replaceAllUsesWith(nCI);
  } else if (LHS->getName().substr(0, 14) == "__amdil_imul24") {
#if LLVM_VERSION >= 2500
    BinaryOperator *mulOp =
      BinaryOperator::Create(Instruction::Mul, CI->getOperand(0),
          CI->getOperand(1), "imul24", CI);
#else
    BinaryOperator *mulOp =
      BinaryOperator::Create(Instruction::Mul, CI->getOperand(1),
          CI->getOperand(2), "imul24", CI);
#endif
    CI->replaceAllUsesWith(mulOp);
  } else if (LHS->getName().substr(0, 19) == "__amdil_imul24_high") {
#if LLVM_VERSION >= 2500
    Type *aType = CI->getOperand(0)->getType();

    bool isVector = aType->isVectorTy();
    int numEle = isVector ? dyn_cast<VectorType>(aType)->getNumElements() : 1;
    std::vector<Type*> callTypes;
    callTypes.push_back(CI->getOperand(0)->getType());
    callTypes.push_back(CI->getOperand(1)->getType());
    FunctionType *funcType =
      FunctionType::get(CI->getOperand(0)->getType(), callTypes, false);
#else
    Type *aType = CI->getOperand(1)->getType();

    bool isVector = aType->isVectorTy();
    int numEle = isVector ? dyn_cast<VectorType>(aType)->getNumElements() : 1;
    std::vector<Type*> callTypes;
    callTypes.push_back(CI->getOperand(1)->getType());
    callTypes.push_back(CI->getOperand(2)->getType());
    FunctionType *funcType =
      FunctionType::get(CI->getOperand(1)->getType(), callTypes, false);
#endif
    std::string name = "__amdil_imul_high";
    if (isVector) {
      name += "_v" + itostr(numEle) + "i32";
    } else {
      name += "_i32";
    }
    Function *Func = dyn_cast<Function>(
                       CI->getParent()->getParent()->getParent()->
                       getOrInsertFunction(llvm::StringRef(name), funcType));
    Value *Operands[2] = {
#if LLVM_VERSION >= 2500
      CI->getOperand(0),
      CI->getOperand(1)
#else
      CI->getOperand(1),
      CI->getOperand(2)
#endif
    };
    CallInst *nCI = CallInst::Create(Func, Operands, "imul24_high");
    nCI->insertBefore(CI);
    CI->replaceAllUsesWith(nCI);
  }
}

bool 
AMDILPeepholeOpt::isRWGLocalOpt(CallInst *CI) 
{
  return (CI != NULL && mRWGOpt
#if LLVM_VERSION >= 2500
          && CI->getOperand(CI->getNumOperands() - 1)->getName() 
#else
          && CI->getOperand(0)->getName() 
#endif
          == "__amdil_get_local_size_int");
}

void 
AMDILPeepholeOpt::expandRWGLocalOpt(CallInst *CI) 
{
  assert(isRWGLocalOpt(CI) &&
         "This optmization only works when the call inst is get_local_size!");
  std::vector<Constant *> consts;
  for (uint32_t x = 0; x < 3; ++x) {
    uint32_t val = mSTM->getGlobalManager()->getLocal(mF->getName(), x);
    consts.push_back(ConstantInt::get(Type::getInt32Ty(*mCTX), val));
  }
  consts.push_back(ConstantInt::get(Type::getInt32Ty(*mCTX), 0));
  Value *cVec = ConstantVector::get(consts);
  CI->replaceAllUsesWith(cVec);
  ++LocalFuncs;
  return;
}

bool 
AMDILPeepholeOpt::convertAccurateDivide(CallInst *CI) 
{
  if (!CI) {
    return false;
  }
  if (mSTM->device()->getGeneration() == AMDILDeviceInfo::HD6XXX
      && (mSTM->getDeviceName() == "cayman"
          || mSTM->getDeviceName() == "kauai"
          || mSTM->getDeviceName() == "trinity")) {
    return false;
  }
#if LLVM_VERSION >= 2500
  return CI->getOperand(CI->getNumOperands() - 1)->getName().substr(0, 20) 
#else
  return CI->getOperand(0)->getName().substr(0, 20) 
#endif
      == "__amdil_improved_div";
}

void 
AMDILPeepholeOpt::expandAccurateDivide(CallInst *CI) 
{
  assert(convertAccurateDivide(CI)
         && "expanding accurate divide can only happen if it is expandable!");
  BinaryOperator *divOp =
#if LLVM_VERSION >= 2500
    BinaryOperator::Create(Instruction::FDiv, CI->getOperand(0),
                           CI->getOperand(1), "fdiv32", CI);
#else
    BinaryOperator::Create(Instruction::FDiv, CI->getOperand(1),
                           CI->getOperand(2), "fdiv32", CI);
#endif
  CI->replaceAllUsesWith(divOp);
}

bool
AMDILPeepholeOpt::propagateSamplerInst(CallInst *CI)
{
  if (optLevel != CodeGenOpt::None) {
    return false;
  }

  if (!CI) {
    return false;
  }

  unsigned funcNameIdx = 0;
#if LLVM_VERSION >= 2500
  funcNameIdx = CI->getNumOperands() - 1;
#endif
  StringRef calleeName = CI->getOperand(funcNameIdx)->getName();
  if (calleeName != "__amdil_image2d_read_norm"
   && calleeName != "__amdil_image2d_read_unnorm"
   && calleeName != "__amdil_image3d_read_norm"
   && calleeName != "__amdil_image3d_read_unnorm") {
    return false;
  }

  unsigned samplerIdx = 2;
#if LLVM_VERSION >= 2500
  samplerIdx = 1;
#endif
  Value *sampler = CI->getOperand(samplerIdx);
  LoadInst *lInst = dyn_cast<LoadInst>(sampler);
  if (!lInst) {
    return false;
  }

  if (lInst->getPointerAddressSpace() != AMDILAS::PRIVATE_ADDRESS) {
    return false;
  }

  GlobalVariable *gv = dyn_cast<GlobalVariable>(lInst->getPointerOperand());
  // If we are loading from what is not a global value, then we
  // fail and return.
  if (!gv) {
    return false;
  }

  // If we don't have an initializer or we have an initializer and
  // the initializer is not a 32bit integer, we fail.
  if (!gv->hasInitializer() 
      || !gv->getInitializer()->getType()->isIntegerTy(32)) {
      return false;
  }

  // Now that we have the global variable initializer, lets replace
  // all uses of the load instruction with the samplerVal and
  // reparse the __amdil_is_constant() function.
  Constant *samplerVal = gv->getInitializer();
  lInst->replaceAllUsesWith(samplerVal);
  return true;
}

bool 
AMDILPeepholeOpt::doInitialization(Module &M) 
{
  return false;
}

bool 
AMDILPeepholeOpt::doFinalization(Module &M) 
{
  return false;
}

void 
AMDILPeepholeOpt::getAnalysisUsage(AnalysisUsage &AU) const 
{
  AU.addRequired<MachineFunctionAnalysis>();
  FunctionPass::getAnalysisUsage(AU);
  AU.setPreservesAll();
}
