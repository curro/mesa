//====-- AMDILMCAsmInfo.cpp - AMD IL asm properties --====//
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
#include "AMDILMCAsmInfo.h"
#include "AMDILLLVMPC.h"
#ifndef NULL
#define NULL 0
#endif

using namespace llvm;
AMDILMCAsmInfo::AMDILMCAsmInfo(const Triple &Triple) : MCAsmInfo()
{
  //===------------------------------------------------------------------===//
  HasSubsectionsViaSymbols = true;
  HasMachoZeroFillDirective = false;
#if LLVM_VERSION >= 2500
  HasMachoTBSSDirective = false;
#endif
  HasStaticCtorDtorReferenceInStaticMode = false;
#if LLVM_VERSION >= 2500
  LinkerRequiresNonEmptyDwarfLines = true;
#endif
  MaxInstLength = 16;
  PCSymbol = "$";
  SeparatorString = "\n";
  CommentColumn = 40;
  CommentString = ";";
#if LLVM_VERSION >= 2500
  LabelSuffix = ":";
#endif
  GlobalPrefix = "@";
  PrivateGlobalPrefix = ";.";
  LinkerPrivateGlobalPrefix = "!";
  InlineAsmStart = ";#ASMSTART";
  InlineAsmEnd = ";#ASMEND";
  AssemblerDialect = 0;
  AllowQuotesInName = false;
  AllowNameToStartWithDigit = false;
#if LLVM_VERSION >= 2500
  AllowPeriodsInName = false;
#endif

  //===--- Data Emission Directives -------------------------------------===//
  ZeroDirective = ".zero";
  AsciiDirective = ".ascii\t";
  AscizDirective = ".asciz\t";
  Data8bitsDirective = ".byte\t";
  Data16bitsDirective = ".short\t";
  Data32bitsDirective = ".long\t";
  Data64bitsDirective = ".quad\t";
  GPRel32Directive = NULL;
  SunStyleELFSectionSwitchSyntax = true;
  UsesELFSectionDirectiveForBSS = true;
  HasMicrosoftFastStdCallMangling = false;

  //===--- Alignment Information ----------------------------------------===//
  AlignDirective = ".align\t";
  AlignmentIsInBytes = true;
  TextAlignFillValue = 0;

  //===--- Global Variable Emission Directives --------------------------===//
  GlobalDirective = ".global";
  ExternDirective = ".extern";
  HasSetDirective = false;
#if LLVM_VERSION >= 2500
  // TODO: This makes the symbol definition have the math instead
  // of the symbol use. This could be disabled and handled as it
  // would simplify the patching code in AMDILMDParser.cpp.
  HasAggressiveSymbolFolding = true;
  LCOMMDirectiveType = LCOMM::None;
#endif
  COMMDirectiveAlignmentIsInBytes = false;
  // TODO: This generates .type @__OpenCL_<name>_kernel,@function
  // and .size @__OpenCL_<name>_kernel, ;.<tmp>-@__OpenCL_<name>_kernel, 
  // which is not handled in AMDILMDParser.cpp.
  HasDotTypeDotSizeDirective = false;
  HasSingleParameterDotFile = true;
  HasNoDeadStrip = true;
#if LLVM_VERSION >= 2500
  HasSymbolResolver = false;
#endif
  WeakRefDirective = ".weakref\t";
  WeakDefDirective = ".weakdef\t";
  LinkOnceDirective = NULL;
  HiddenVisibilityAttr = MCSA_Hidden;
#if LLVM_VERSION >= 2500
  HiddenDeclarationVisibilityAttr = MCSA_Hidden;
#endif
  ProtectedVisibilityAttr = MCSA_Protected;

  //===--- Dwarf Emission Directives -----------------------------------===//
  HasLEB128 = true;
#if LLVM_VERSION < 2500
  HasDotLocAndDotFile = false;
#endif
  SupportsDebugInformation = true;
  ExceptionsType = ExceptionHandling::None;
  DwarfUsesInlineInfoSection = false;
  DwarfSectionOffsetDirective = ".offset";
#if LLVM_VERSION >= 2500
  DwarfUsesLabelOffsetForRanges = true;
#endif

  //===--- CBE Asm Translation Table -----------------------------------===//
  AsmTransCBE = NULL;
}
const char*
AMDILMCAsmInfo::getDataASDirective(unsigned int Size, unsigned int AS) const
{
  switch (AS) {
    default:
      return NULL;
    case 0:
      return NULL;
  };
  return NULL;
}

MCSection*
AMDILMCAsmInfo::getNonexecutableStackSection(MCContext &CTX)
{
  return NULL;
}
