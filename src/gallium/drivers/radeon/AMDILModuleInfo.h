//===--------------- AMDILMachineModuleInfo.h -------------------*- C++ -*-===//
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
// to the U.S. Export Administration Regulations (“EAR”), (15 C.F.R. Sections
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
// Industry and Security’s website at http://www.bis.doc.gov/.
//
//==-----------------------------------------------------------------------===//
//
// This is an MMI implementation for AMDIL targets.
//
//===----------------------------------------------------------------------===//

#ifndef _AMDIL_MACHINE_MODULE_INFO_H_
#define _AMDIL_MACHINE_MODULE_INFO_H_
#include "AMDIL.h"
#include "AMDILKernel.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <set>

namespace llvm {
  class AMDILKernel;
  class Argument;
  class TypeSymbolTable;
  class GlobalValue;
  class MachineFunction;
  class GlobalValue;

  class AMDILModuleInfo : public MachineModuleInfoImpl {
    protected:
      const MachineModuleInfo *mMMI;
    public:
      AMDILModuleInfo(const MachineModuleInfo &);
      virtual ~AMDILModuleInfo();

      void processModule(const Module *MF, const AMDILTargetMachine* mTM);

      /// Process the given module and parse out the global variable metadata passed
      /// down from the frontend-compiler

      /// Returns true if the image ID corresponds to a read only image.
      bool isReadOnlyImage(const llvm::StringRef &name, uint32_t iID) const;

      /// Returns true if the image ID corresponds to a write only image.
      bool isWriteOnlyImage(const llvm::StringRef &name, uint32_t iID) const;

      /// Gets the group size of the kernel for the given dimension.
      uint32_t getRegion(const llvm::StringRef &name, uint32_t dim) const;

      /// Get the offset of the array for the kernel.
      int32_t getArrayOffset(const llvm::StringRef &name) const;

      /// Get the offset of the const memory for the kernel.
      int32_t getConstOffset(const llvm::StringRef &name) const;

      /// Get the boolean value if this particular constant uses HW or not.
      bool getConstHWBit(const llvm::StringRef &name) const;

      /// Get a reference to the kernel metadata information for the given function
      /// name.
      AMDILKernel *getKernel(const llvm::StringRef &name);
      bool isKernel(const llvm::StringRef &name) const;

      /// Dump the data section to the output stream for the given kernel.
      //void dumpDataSection(OSTREAM_TYPE &O, AMDILKernelManager *km);

      /// Iterate through the constants that are global to the compilation unit.
      StringMap<AMDILConstPtr>::iterator consts_begin();
      StringMap<AMDILConstPtr>::iterator consts_end();

      /// Query if the kernel has a byte store.
      bool byteStoreExists(llvm::StringRef S) const;

      /// Query if the constant pointer is an argument.
      bool isConstPtrArgument(const AMDILKernel *krnl, const llvm::StringRef &arg);

      /// Query if the constant pointer is an array that is globally scoped.
      bool isConstPtrArray(const AMDILKernel *krnl, const llvm::StringRef &arg);

      /// Query the size of the constant pointer.
      uint32_t getConstPtrSize(const AMDILKernel *krnl, const llvm::StringRef &arg);

      /// Query the offset of the constant pointer.
      uint32_t getConstPtrOff(const AMDILKernel *krnl, const llvm::StringRef &arg);

      /// Query the constant buffer number for a constant pointer.
      uint32_t getConstPtrCB(const AMDILKernel *krnl, const llvm::StringRef &arg);

      /// Query the Value* that the constant pointer originates from.
      const Value *getConstPtrValue(const AMDILKernel *krnl, const llvm::StringRef &arg);

      /// Get the ID of the argument.
      int32_t getArgID(const Argument *arg);

      /// Get the unique function ID for the specific function name and create a new
      /// unique ID if it is not found.
      uint32_t getOrCreateFunctionID(const GlobalValue* func);
      uint32_t getOrCreateFunctionID(const std::string& func);

      /// Calculate the offsets of the constant pool for the given kernel and
      /// machine function.
      void calculateCPOffsets(const MachineFunction *MF, AMDILKernel *krnl);

      void add_printf_offset(uint32_t offset) { mPrintfOffset += offset; }
      uint32_t get_printf_offset() { return mPrintfOffset; }

    private:
      /// Various functions that parse global value information and store them in
      /// the global manager. This approach is used instead of dynamic parsing as it
      /// might require more space, but should allow caching of data that gets
      /// requested multiple times.
      AMDILKernelAttr parseSGV(const GlobalValue *GV);
      AMDILLocalArg  parseLVGV(const GlobalValue *GV);
      void parseGlobalAnnotate(const GlobalValue *G);
      void parseImageAnnotate(const GlobalValue *G);
      void parseConstantPtrAnnotate(const GlobalValue *G);
      void printConstantValue(const Constant *CAval,
          OSTREAM_TYPE& O,
          bool asByte);
      void parseKernelInformation(const Value *V);
      void parseAutoArray(const GlobalValue *G, bool isRegion);
      void parseConstantPtr(const GlobalValue *G);
      void allocateGlobalCB();
      bool checkConstPtrsUseHW(Module::const_iterator *F);

      llvm::StringMap<AMDILKernel*> mKernels;
      llvm::StringMap<AMDILKernelAttr> mKernelArgs;
      llvm::StringMap<AMDILArrayMem> mArrayMems;
      llvm::StringMap<AMDILConstPtr> mConstMems;
      llvm::StringMap<AMDILLocalArg> mLocalArgs;
      llvm::StringMap<uint32_t> mFuncNames;
      llvm::DenseMap<const GlobalValue*, uint32_t> mFuncPtrNames;
      llvm::DenseMap<uint32_t, llvm::StringRef> mImageNameMap;
      std::set<llvm::StringRef> mByteStore;
      std::set<llvm::StringRef> mIgnoreStr;
      llvm::DenseMap<const Argument *, int32_t> mArgIDMap;
      const TypeSymbolTable *symTab;
      const AMDILSubtarget *mSTM;
      size_t mOffset;
      uint32_t mReservedBuffs;
      uint32_t mCurrentCPOffset;
      uint32_t mPrintfOffset;
  };



} // end namespace llvm

#endif // _AMDIL_COFF_MACHINE_MODULE_INFO_H_

