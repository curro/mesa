//===------------ AMDILInstrInfo.td - AMDIL Target ------*-tablegen-*------===//
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
// This file describes the AMDIL instructions in TableGen format.
//
//===----------------------------------------------------------------------===//
// AMDIL Instruction Predicate Definitions
// Predicate that is set to true if the hardware supports double precision
// divide
def HasHWDDiv                 : Predicate<"Subtarget.device()"
                           "->getGeneration() > AMDILDeviceInfo::HD4XXX && "
              "Subtarget.device()->usesHardware(AMDILDeviceInfo::DoubleOps)">;

// Predicate that is set to true if the hardware supports double, but not double
// precision divide in hardware
def HasSWDDiv             : Predicate<"Subtarget.device()"
                           "->getGeneration() == AMDILDeviceInfo::HD4XXX &&"
              "Subtarget.device()->usesHardware(AMDILDeviceInfo::DoubleOps)">;

// Predicate that is set to true if the hardware support 24bit signed
// math ops. Otherwise a software expansion to 32bit math ops is used instead.
def HasHWSign24Bit          : Predicate<"Subtarget.device()"
                            "->getGeneration() > AMDILDeviceInfo::HD5XXX">;

// Predicate that is set to true if 64bit operations are supported or not
def HasHW64Bit              : Predicate<"Subtarget.device()"
                            "->usesHardware(AMDILDeviceInfo::LongOps)">;
def HasSW64Bit              : Predicate<"Subtarget.device()"
                            "->usesSoftware(AMDILDeviceInfo::LongOps)">;

// Predicate that is set to true if the timer register is supported
def HasTmrRegister          : Predicate<"Subtarget.device()"
                            "->isSupported(AMDILDeviceInfo::TmrReg)">;
// Predicate that is true if we are at least evergreen series
def HasDeviceIDInst         : Predicate<"Subtarget.device()"
                            "->getGeneration() >= AMDILDeviceInfo::HD5XXX">;

// Predicate that is true if we have region address space.
def hasRegionAS             : Predicate<"Subtarget.device()"
                            "->usesHardware(AMDILDeviceInfo::RegionMem)">;

// Predicate that is false if we don't have region address space.
def noRegionAS             : Predicate<"!Subtarget.device()"
                            "->isSupported(AMDILDeviceInfo::RegionMem)">;


// Predicate that is set to true if 64bit Mul is supported in the IL or not
def HasHW64Mul              : Predicate<"Subtarget.calVersion()" 
                                          ">= CAL_VERSION_SC_139"
                                          "&& Subtarget.device()"
                                          "->getGeneration() >="
                                          "AMDILDeviceInfo::HD5XXX">;
def HasSW64Mul              : Predicate<"Subtarget.calVersion()" 
                                          "< CAL_VERSION_SC_139">;
// Predicate that is set to true if 64bit Div/Mod is supported in the IL or not
def HasHW64DivMod           : Predicate<"Subtarget.device()"
                            "->usesHardware(AMDILDeviceInfo::HW64BitDivMod)">;
def HasSW64DivMod           : Predicate<"Subtarget.device()"
                            "->usesSoftware(AMDILDeviceInfo::HW64BitDivMod)">;

// Predicate that is set to true if 64bit pointer are used.
def Has64BitPtr             : Predicate<"Subtarget.is64bit()">;
def Has32BitPtr             : Predicate<"!Subtarget.is64bit()">;
//===--------------------------------------------------------------------===//
// Custom Operands
//===--------------------------------------------------------------------===//
include "AMDILOperands.td"

//===--------------------------------------------------------------------===//
// Custom Selection DAG Type Profiles
//===--------------------------------------------------------------------===//
include "AMDILProfiles.td"

//===--------------------------------------------------------------------===//
// Custom Selection DAG Nodes
//===--------------------------------------------------------------------===//
include "AMDILNodes.td"

//===--------------------------------------------------------------------===//
// Custom Pattern DAG Nodes
//===--------------------------------------------------------------------===//
include "AMDILPatterns.td"

//===----------------------------------------------------------------------===//
// Instruction format classes
//===----------------------------------------------------------------------===//
include "AMDILFormats.td"

//===--------------------------------------------------------------------===//
// Multiclass Instruction formats
//===--------------------------------------------------------------------===//
include "AMDILMultiClass.td"

//===--------------------------------------------------------------------===//
// Intrinsics support
//===--------------------------------------------------------------------===//
include "AMDILIntrinsics.td"

//===--------------------------------------------------------------------===//
// Instructions support
//===--------------------------------------------------------------------===//
include "AMDILInstructions.td"

//===--------------------------------------------------------------------===//
// Instruction Pattern support - This Must be the last include in the file
// as it requires items defined in other files
//===--------------------------------------------------------------------===//
include "AMDILInstrPatterns.td"

