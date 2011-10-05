//==-----------------------------------------------------------------------===//
//
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
// @file AMDILKernelManager.h
// @details Class that handles the metadata/abi management for the
// ASM printer. Handles the parsing and generation of the metadata
// for each kernel and keeps track of its arguments.
//
#ifndef _AMDILKERNELMANAGER_H_
#define _AMDILKERNELMANAGER_H_
#include "AMDIL.h"
#include "AMDILDevice.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/ValueMap.h"
#include "llvm/Function.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include <string>
#include <set>
#include <map>
#define IMAGETYPE_2D 0
#define IMAGETYPE_3D 1
#define RESERVED_LIT_COUNT 6

namespace llvm {
class AMDILGlobalManager;
class AMDILSubtarget;
class AMDILMachineFunctionInfo;
class AMDILTargetMachine;
class AMDILAsmPrinter;
class StructType;
class Value;
class TypeSymbolTable;
class MachineFunction;
class MachineInstr;
class ConstantFP;
class PrintfInfo;


class AMDILKernelManager {
public:
  typedef enum {
    RELEASE_ONLY,
    DEBUG_ONLY,
    ALWAYS
  } ErrorMsgEnum;
  AMDILKernelManager(AMDILTargetMachine *TM, AMDILGlobalManager *GM);
  virtual ~AMDILKernelManager();
  
  /// Clear the state of the KernelManager putting it in its most initial state.
  void clear();
  void setMF(MachineFunction *MF);

  /// Process the specific kernel parsing out the parameter information for the
  /// kernel.
  void processArgMetadata(OSTREAM_TYPE &O,
                          uint32_t buf, bool kernel);


  /// Prints the header for the kernel which includes the groupsize declaration
  /// and calculation of the local/group/global id's.
  void printHeader(AMDILAsmPrinter *AsmPrinter, OSTREAM_TYPE &O,
                   const std::string &name);

  virtual void printDecls(AMDILAsmPrinter *AsmPrinter, OSTREAM_TYPE &O);
  virtual void printGroupSize(OSTREAM_TYPE &O);

  /// Copies the data from the runtime setup constant buffers into registers so
  /// that the program can correctly access memory or data that was set by the
  /// host program.
  void printArgCopies(OSTREAM_TYPE &O, AMDILAsmPrinter* RegNames);

  /// Prints out the end of the function.
  void printFooter(OSTREAM_TYPE &O);
  
  /// Prints out the metadata for the specific function depending if it is a
  /// kernel or not.
  void printMetaData(OSTREAM_TYPE &O, uint32_t id, bool isKernel = false);
  
  /// Set bool value on whether to consider the function a kernel or a normal
  /// function.
  void setKernel(bool kernel);

  /// Set the unique ID of the kernel/function.
  void setID(uint32_t id);

  /// Set the name of the kernel/function.
  void setName(const std::string &name);

  /// Flag to specify whether the function is a kernel or not.
  bool isKernel();

  /// Flag that specifies whether this function has a kernel wrapper.
  bool wasKernel();

  void getIntrinsicSetup(AMDILAsmPrinter *AsmPrinter, OSTREAM_TYPE &O); 

  // Returns whether a compiler needs to insert a write to memory or not.
  bool useCompilerWrite(const MachineInstr *MI);

  // Set the flag that there exists an image write.
  void setImageWrite();
  void setOutputInst();

  const char *getTypeName(const Type *name, const char * symTab);

  void emitLiterals(OSTREAM_TYPE &O);

  // Set the uav id for the specific pointer value.  If value is NULL, then the
  // ID sets the default ID.
  void setUAVID(const Value *value, uint32_t ID);

  // Get the UAV id for the specific pointer value.
  uint32_t getUAVID(const Value *value);

private:

  /// Helper function that prints the actual metadata and should only be called
  /// by printMetaData.
  void printKernelArgs(OSTREAM_TYPE &O);
  void printCopyStructPrivate(const StructType *ST,
                              OSTREAM_TYPE &O,
                              size_t stackSize,
                              uint32_t Buffer,
                              uint32_t mLitIdx,
                              uint32_t &counter);
  virtual void
  printConstantToRegMapping(AMDILAsmPrinter *RegNames,
                            uint32_t &LII,
                            OSTREAM_TYPE &O,
                            uint32_t &counter,
                            uint32_t Buffer,
                            uint32_t n,
                            const char *lit = NULL,
                            uint32_t fcall = 0,
                            bool isImage = false,
                            bool isHWCB = false);
  void updatePtrArg(llvm::Function::const_arg_iterator Ip,
                    int numWriteImages,
                    int raw_uav_buffer,
                    int counter,
                    bool isKernel,
                    const Function *F);
  /// Name of the current kernel.
  std::string mName;
  uint32_t mUniqueID;
  bool mIsKernel;
  bool mWasKernel;
  bool mCompilerWrite;
  /// Flag to specify if an image write has occured or not in order to not add a
  /// compiler specific write if no other writes to memory occured.
  bool mHasImageWrite;
  bool mHasOutputInst;
  
  /// Map from const Value * to UAV ID.
  std::map<const Value *, uint32_t> mValueIDMap;

  AMDILTargetMachine * mTM;
  const AMDILSubtarget * mSTM;
  AMDILGlobalManager * mGM;
  /// This is the global offset of the printf string id's.
  MachineFunction *mMF;
  AMDILMachineFunctionInfo *mMFI;
}; // class AMDILKernelManager

} // llvm namespace
#endif // _AMDILKERNELMANAGER_H_
