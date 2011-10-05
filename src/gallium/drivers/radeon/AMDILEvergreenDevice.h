//==- AMDILEvergreenDevice.h - Define Evergreen Device for AMDIL -*- C++ -*--=//
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
#ifndef _AMDILEVERGREENDEVICE_H_
#define _AMDILEVERGREENDEVICE_H_
#include "AMDILDevice.h"
#include "AMDILSubtarget.h"
namespace llvm {
  class AMDILSubtarget;
//===----------------------------------------------------------------------===//
// Evergreen generation of devices and their respective sub classes
//===----------------------------------------------------------------------===//


// The AMDILEvergreenDevice is the base device class for all of the Evergreen
// series of cards. This class contains information required to differentiate
// the Evergreen device from the generic AMDILDevice. This device represents
// that capabilities of the 'Juniper' cards, also known as the HD57XX.
class AMDILEvergreenDevice : public AMDILDevice {
public:
  AMDILEvergreenDevice(AMDILSubtarget *ST);
  virtual ~AMDILEvergreenDevice();
  virtual size_t getMaxLDSSize() const;
  virtual size_t getMaxGDSSize() const;
  virtual size_t getWavefrontSize() const;
  virtual uint32_t getGeneration() const;
  virtual uint32_t getMaxNumUAVs() const;
  virtual uint32_t getResourceID(uint32_t) const;
  virtual FunctionPass*
    getIOExpansion(TargetMachine&, CodeGenOpt::Level) const;
  virtual AsmPrinter*
    getAsmPrinter(AMDIL_ASM_PRINTER_ARGUMENTS) const;
  virtual FunctionPass*
    getPointerManager(TargetMachine&, CodeGenOpt::Level) const;
protected:
  virtual void setCaps();
}; // AMDILEvergreenDevice

// The AMDILCypressDevice is similiar to the AMDILEvergreenDevice, except it has
// support for double precision operations. This device is used to represent
// both the Cypress and Hemlock cards, which are commercially known as HD58XX
// and HD59XX cards.
class AMDILCypressDevice : public AMDILEvergreenDevice {
public:
  AMDILCypressDevice(AMDILSubtarget *ST);
  virtual ~AMDILCypressDevice();
private:
  virtual void setCaps();
}; // AMDILCypressDevice


// The AMDILCedarDevice is the class that represents all of the 'Cedar' based
// devices. This class differs from the base AMDILEvergreenDevice in that the
// device is a ~quarter of the 'Juniper'. These are commercially known as the
// HD54XX and HD53XX series of cards.
class AMDILCedarDevice : public AMDILEvergreenDevice {
public:
  AMDILCedarDevice(AMDILSubtarget *ST);
  virtual ~AMDILCedarDevice();
  virtual size_t getWavefrontSize() const;
private:
  virtual void setCaps();
}; // AMDILCedarDevice

// The AMDILRedwoodDevice is the class the represents all of the 'Redwood' based
// devices. This class differs from the base class, in that these devices are
// considered about half of a 'Juniper' device. These are commercially known as
// the HD55XX and HD56XX series of cards.
class AMDILRedwoodDevice : public AMDILEvergreenDevice {
public:
  AMDILRedwoodDevice(AMDILSubtarget *ST);
  virtual ~AMDILRedwoodDevice();
  virtual size_t getWavefrontSize() const;
private:
  virtual void setCaps();
}; // AMDILRedwoodDevice
  
} // namespace llvm
#endif // _AMDILEVERGREENDEVICE_H_
