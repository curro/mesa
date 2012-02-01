//==-- AMDIL7XXDevice.h - Define 7XX Device Device for AMDIL ---*- C++ -*--===//
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
// Interface for the subtarget data classes.
//
//===----------------------------------------------------------------------===//
// This file will define the interface that each generation needs to
// implement in order to correctly answer queries on the capabilities of the
// specific hardware.
//===----------------------------------------------------------------------===//
#ifndef _AMDIL7XXDEVICEIMPL_H_
#define _AMDIL7XXDEVICEIMPL_H_
#include "AMDILDevice.h"
#include "AMDILSubtarget.h"
namespace llvm {
class AMDILSubtarget;

//===----------------------------------------------------------------------===//
// 7XX generation of devices and their respective sub classes
//===----------------------------------------------------------------------===//

// The AMDIL7XXDevice class represents the generic 7XX device. All 7XX
// devices are derived from this class. The AMDIL7XX device will only
// support the minimal features that are required to be considered OpenCL 1.0
// compliant and nothing more.
class AMDIL7XXDevice : public AMDILDevice {
public:
  AMDIL7XXDevice(AMDILSubtarget *ST);
  virtual ~AMDIL7XXDevice();
  virtual size_t getMaxLDSSize() const;
  virtual size_t getWavefrontSize() const;
  virtual uint32_t getGeneration() const;
  virtual uint32_t getResourceID(uint32_t DeviceID) const;
  virtual uint32_t getMaxNumUAVs() const;
  FunctionPass*
    getIOExpansion(TargetMachine& AMDIL_OPT_LEVEL_DECL) const;
  AsmPrinter* 
    getAsmPrinter(AMDIL_ASM_PRINTER_ARGUMENTS) const;
  FunctionPass*
    getPointerManager(TargetMachine& AMDIL_OPT_LEVEL_DECL) const;

protected:
  virtual void setCaps();
}; // AMDIL7XXDevice

// The AMDIL770Device class represents the RV770 chip and it's
// derivative cards. The difference between this device and the base
// class is this device device adds support for double precision
// and has a larger wavefront size.
class AMDIL770Device : public AMDIL7XXDevice {
public:
  AMDIL770Device(AMDILSubtarget *ST);
  virtual ~AMDIL770Device();
  virtual size_t getWavefrontSize() const;
private:
  virtual void setCaps();
}; // AMDIL770Device

// The AMDIL710Device class derives from the 7XX base class, but this
// class is a smaller derivative, so we need to overload some of the
// functions in order to correctly specify this information.
class AMDIL710Device : public AMDIL7XXDevice {
public:
  AMDIL710Device(AMDILSubtarget *ST);
  virtual ~AMDIL710Device();
  virtual size_t getWavefrontSize() const;
}; // AMDIL710Device

} // namespace llvm
#endif // _AMDILDEVICEIMPL_H_
