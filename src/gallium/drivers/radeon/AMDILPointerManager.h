//===-------- AMDILPointerManager.h - Manage Pointers for HW ------------===//
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
// The AMDIL Pointer Manager is a class that does all the checking for
// different pointer characteristics. Pointers have attributes that need
// to be attached to them in order to correctly codegen them efficiently.
// This class will analyze the pointers of a function and then traverse the uses
// of the pointers and determine if a pointer can be cached, should belong in
// the arena, and what UAV it should belong to. There are seperate classes for
// each unique generation of devices. This pass only works in SSA form.
//===----------------------------------------------------------------------===//
#ifndef _AMDIL_POINTER_MANAGER_H_
#define _AMDIL_POINTER_MANAGER_H_
#undef DEBUG_TYPE
#undef DEBUGME
#define DEBUG_TYPE "PointerManager"
#if !defined(NDEBUG)
#define DEBUGME (DebugFlag && isCurrentDebugType(DEBUG_TYPE))
#else
#define DEBUGME (false)
#endif
#include "AMDIL.h"
#include "AMDILUtilityFunctions.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunctionAnalysis.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetMachine.h"
#include <set>
#include <map>
#include <list>
#include <queue>

namespace llvm {
  class Value;
  class MachineBasicBlock;
  // Typedefing the multiple different set types to that it is
  // easier to read what each set is supposed to handle. This
  // also allows it easier to track which set goes to which
  // argument in a function call.
  typedef std::set<const Value*> PtrSet;

  // A Byte set is the set of all base pointers that must
  // be allocated to the arena path.
  typedef PtrSet ByteSet;

  // A Raw set is the set of all base pointers that can be
  // allocated to the raw path.
  typedef PtrSet RawSet;

  // A cacheable set is the set of all base pointers that
  // are deamed cacheable based on annotations or
  // compiler options.
  typedef PtrSet CacheableSet;

  // A conflict set is a set of all base pointers whose 
  // use/def chains conflict with another base pointer.
  typedef PtrSet ConflictSet;

  // An image set is a set of all read/write only image pointers.
  typedef PtrSet ImageSet;

  // An append set is a set of atomic counter base pointers
  typedef std::vector<const Value*> AppendSet;

  // A ConstantSet is a set of constant pool instructions
  typedef std::set<MachineInstr*> CPoolSet;

  // A CacheableInstSet set is a set of instructions that are cachable
  // even if the pointer is not generally cacheable.
  typedef std::set<MachineInstr*> CacheableInstrSet;

  // A pair that maps a virtual register to the equivalent base
  // pointer value that it was derived from.
  typedef std::pair<unsigned, const Value*> RegValPair;

  // A map that maps between the base pointe rvalue and an array
  // of instructions that are part of the pointer chain. A pointer
  // chain is a recursive def/use chain of all instructions that don't
  // store data to memory unless the pointer is the data being stored.
  typedef std::map<const Value*, std::vector<MachineInstr*> > PtrIMap;

  // A map that holds a set of all base pointers that are used in a machine
  // instruction. This helps to detect when conflict pointers are found
  // such as when pointer subtraction occurs.
  typedef std::map<MachineInstr*, PtrSet> InstPMap;

  // A map that holds the frame index to RegValPair so that writes of 
  // pointers to the stack can be tracked.
  typedef std::map<unsigned, RegValPair > FIPMap;

  // A small vector impl that holds all of the register to base pointer 
  // mappings for a given function.
  typedef std::map<unsigned, RegValPair> RVPVec;



  // The default pointer manager. This handles pointer 
  // resource allocation for default ID's only. 
  // There is no special processing.
  class AMDILPointerManager : public MachineFunctionPass
  {
    public:
      AMDILPointerManager(
          TargetMachine &tm
          AMDIL_OPT_LEVEL_DECL);
      virtual ~AMDILPointerManager();
      virtual const char*
        getPassName() const;
      virtual bool
        runOnMachineFunction(MachineFunction &F);
      virtual void
        getAnalysisUsage(AnalysisUsage &AU) const;
      static char ID;
    protected:
      bool mDebug;
    private:
      TargetMachine &TM;
  }; // class AMDILPointerManager

  // The pointer manager for Evergreen and Northern Island
  // devices. This pointer manager allocates and trackes
  // cached memory, arena resources, raw resources and
  // whether multi-uav is utilized or not.
  class AMDILEGPointerManager : public AMDILPointerManager
  {
    public:
      AMDILEGPointerManager(
          TargetMachine &tm
          AMDIL_OPT_LEVEL_DECL);
      virtual ~AMDILEGPointerManager();
      virtual const char*
        getPassName() const;
      virtual bool
        runOnMachineFunction(MachineFunction &F);
    private:
      TargetMachine &TM;
  }; // class AMDILEGPointerManager

  // Information related to the cacheability of instructions in a basic block.
  // This is used during the parse phase of the pointer algorithm to track
  // the reachability of stores within a basic block.
  class BlockCacheableInfo {
    public:
      BlockCacheableInfo() :
        mStoreReachesTop(false),
        mStoreReachesExit(false),
        mCacheableSet()
    {};

      bool storeReachesTop() const  { return mStoreReachesTop; }
      bool storeReachesExit() const { return mStoreReachesExit; }
      CacheableInstrSet::const_iterator 
        cacheableBegin() const { return mCacheableSet.begin(); }
      CacheableInstrSet::const_iterator 
        cacheableEnd()   const { return mCacheableSet.end(); }

      // mark the block as having a global store that reaches it. This
      // will also set the store reaches exit flag, and clear the list
      // of loads (since they are now reachable by a store.)
      bool setReachesTop() {
        bool changedExit = !mStoreReachesExit;

        if (!mStoreReachesTop)
          mCacheableSet.clear();

        mStoreReachesTop = true;
        mStoreReachesExit = true;
        return changedExit;
      }

      // Mark the block as having a store that reaches the exit of the 
      // block.
      void setReachesExit() {
        mStoreReachesExit = true;
      }

      // If the top or the exit of the block are not marked as reachable
      // by a store, add the load to the list of cacheable loads.
      void addPossiblyCacheableInst(MachineInstr *load) {
        // By definition, if store reaches top, then store reaches exit.
        // So, we only test for exit here.
        // If we have a volatile load we cannot cache it.
        if (mStoreReachesExit || isVolatileInst(load)) {
          return;
        }

        mCacheableSet.insert(load);
      }

    private:
      bool mStoreReachesTop; // Does a global store reach the top of this block?
      bool mStoreReachesExit;// Does a global store reach the exit of this block?
      CacheableInstrSet mCacheableSet; // The set of loads in the block not 
      // reachable by a global store.
  };
  // Map from MachineBasicBlock to it's cacheable load info.
  typedef std::map<MachineBasicBlock*, BlockCacheableInfo> MBBCacheableMap;
} // end llvm namespace
#endif // _AMDIL_POINTER_MANAGER_H_
