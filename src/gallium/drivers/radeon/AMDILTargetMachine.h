//===-- AMDILTargetMachine.h - Define TargetMachine for AMDIL ---*- C++ -*-===//
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
// This file declares the AMDIL specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef AMDILTARGETMACHINE_H_
#define AMDILTARGETMACHINE_H_


#include "AMDIL.h"
#include "AMDILELFWriterInfo.h"
#if LLVM_VERSION < 2500
#include "AMDILFrameInfo.h"
#else
#include "AMDILFrameLowering.h"
#endif
#include "AMDILInstrInfo.h"
#include "AMDILISelLowering.h"
#include "AMDILIntrinsicInfo.h"
#include "AMDILSubtarget.h"

#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetData.h"

namespace llvm
{
    class raw_ostream;

    class AMDILTargetMachine : public LLVMTargetMachine
    {
        private:
        AMDILSubtarget Subtarget;
        const TargetData DataLayout;       // Calculates type size & alignment
#if LLVM_VERSION < 2500
        AMDILFrameInfo FrameLowering;
#else
        AMDILFrameLowering FrameLowering;
#endif
        AMDILInstrInfo InstrInfo;
        AMDILTargetLowering TLInfo;
        AMDILIntrinsicInfo IntrinsicInfo;
        AMDILELFWriterInfo ELFWriterInfo;
        bool mDebugMode;
        CodeGenOpt::Level mOptLevel;

        protected:

        public:
        AMDILTargetMachine(const Target &T,
             StringRef TT, StringRef CPU, StringRef FS,
#if LLVM_VERSION > 3000
             TargetOptions Options,
#endif
             Reloc::Model RM, CodeModel::Model CM
#if LLVM_VERSION > 3000
             ,CodeGenOpt::Level OL
#endif
);

        // Get Target/Subtarget specific information
        virtual AMDILTargetLowering* getTargetLowering() const;
        virtual const AMDILInstrInfo* getInstrInfo() const;
#if LLVM_VERSION < 2500
        virtual const AMDILFrameInfo* getFrameInfo() const;
#else
        virtual const AMDILFrameLowering* getFrameLowering() const;
#endif
        virtual const AMDILSubtarget* getSubtargetImpl() const;
        virtual const AMDILRegisterInfo* getRegisterInfo() const;
        virtual const TargetData* getTargetData() const;
        virtual const AMDILIntrinsicInfo *getIntrinsicInfo() const;
        virtual const AMDILELFWriterInfo *getELFWriterInfo() const;

        // Pass Pipeline Configuration
        virtual bool
            addPreEmitPass(PassManagerBase &PM
#if LLVM_VERSION <= 3000
                           AMDIL_OPT_LEVEL_DECL
#endif
                          );
        virtual bool
          addPreISel(PassManagerBase &PM
#if LLVM_VERSION <= 3000
                     AMDIL_OPT_LEVEL_DECL
#endif
);
        virtual bool
            addInstSelector(PassManagerBase &PM
#if LLVM_VERSION <= 3000
                             AMDIL_OPT_LEVEL_DECL
#endif
);
        virtual bool
            addPreRegAlloc(PassManagerBase &PM
#if LLVM_VERSION <= 3000
                            AMDIL_OPT_LEVEL_DECL
#endif
        );
        virtual bool
            addPostRegAlloc(PassManagerBase &PM
#if LLVM_VERSION <= 3000
                             AMDIL_OPT_LEVEL_DECL
#endif
        );
#if LLVM_VERSION < 2500
        virtual bool
            addPassesToEmitFile(PassManagerBase &, formatted_raw_ostream &,
                                   CodeGenFileType, CodeGenOpt::Level,
                                   bool = true);
        virtual bool
                addCommonCodeGenPasses(PassManagerBase &PM,
                                               CodeGenOpt::Level OptLevel,
                                               bool DisableVerify,
                                               MCContext *&OutContext);
#endif
        void dump(OSTREAM_TYPE &O);
        void setDebug(bool debugMode);
        bool getDebug() const;
        CodeGenOpt::Level getOptLevel() const { return mOptLevel; }


    }; // AMDILTargetMachine

    class TheAMDILTargetMachine : public AMDILTargetMachine {
        public:
        TheAMDILTargetMachine(const Target &T,
                StringRef TT, StringRef CPU, StringRef FS,
                Reloc::Model RM, CodeModel::Model CM);
    }; // TheAMDILTargetMachine

} // end namespace llvm

#endif // AMDILTARGETMACHINE_H_
