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
#include "AMDIL7XXDevice.h"
#include "AMDIL7XXAsmPrinter.h"
#include "AMDILDevice.h"
#include "AMDILIOExpansion.h"
#include "AMDILPointerManager.h"

using namespace llvm;

AMDIL7XXDevice::AMDIL7XXDevice(AMDILSubtarget *ST) : AMDILDevice(ST)
{
  setCaps();
  std::string name = mSTM->getDeviceName();
  if (name == "rv710") {
    mDeviceFlag = OCL_DEVICE_RV710;
  } else if (name == "rv730") {
    mDeviceFlag = OCL_DEVICE_RV730;
  } else {
    mDeviceFlag = OCL_DEVICE_RV770;
  }
}

AMDIL7XXDevice::~AMDIL7XXDevice()
{
}

void AMDIL7XXDevice::setCaps()
{
  mSWBits.set(AMDILDeviceInfo::LocalMem);
}

size_t AMDIL7XXDevice::getMaxLDSSize() const
{
  if (usesHardware(AMDILDeviceInfo::LocalMem)) {
    return MAX_LDS_SIZE_700;
  }
  return 0;
}

size_t AMDIL7XXDevice::getWavefrontSize() const
{
  return AMDILDevice::HalfWavefrontSize;
}

uint32_t AMDIL7XXDevice::getGeneration() const
{
  return AMDILDeviceInfo::HD4XXX;
}

uint32_t AMDIL7XXDevice::getResourceID(uint32_t DeviceID) const
{
  switch (DeviceID) {
  default:
    assert(0 && "ID type passed in is unknown!");
    break;
  case GLOBAL_ID:
  case CONSTANT_ID:
  case RAW_UAV_ID:
  case ARENA_UAV_ID:
    break;
  case LDS_ID:
    if (usesHardware(AMDILDeviceInfo::LocalMem)) {
      return DEFAULT_LDS_ID;
    }
    break;
  case SCRATCH_ID:
    if (usesHardware(AMDILDeviceInfo::PrivateMem)) {
      return DEFAULT_SCRATCH_ID;
    }
    break;
  case GDS_ID:
    assert(0 && "GDS UAV ID is not supported on this chip");
    if (usesHardware(AMDILDeviceInfo::RegionMem)) {
      return DEFAULT_GDS_ID;
    }
    break;
  };

  return 0;
}

uint32_t AMDIL7XXDevice::getMaxNumUAVs() const
{
  return 1;
}

FunctionPass* 
AMDIL7XXDevice::getIOExpansion(
    TargetMachine& TM AMDIL_OPT_LEVEL_DECL) const
{
  return new AMDIL7XXIOExpansion(TM  AMDIL_OPT_LEVEL_VAR);
}

AsmPrinter*
AMDIL7XXDevice::getAsmPrinter(AMDIL_ASM_PRINTER_ARGUMENTS) const
{
  return new AMDIL7XXAsmPrinter(ASM_PRINTER_ARGUMENTS);
}

FunctionPass*
AMDIL7XXDevice::getPointerManager(
    TargetMachine& TM AMDIL_OPT_LEVEL_DECL) const
{
  return new AMDILPointerManager(TM  AMDIL_OPT_LEVEL_VAR);
}

AMDIL770Device::AMDIL770Device(AMDILSubtarget *ST): AMDIL7XXDevice(ST)
{
  setCaps();
}

AMDIL770Device::~AMDIL770Device()
{
}

void AMDIL770Device::setCaps()
{
  if (mSTM->isOverride(AMDILDeviceInfo::DoubleOps)) {
    mSWBits.set(AMDILDeviceInfo::FMA);
    mHWBits.set(AMDILDeviceInfo::DoubleOps);
  }
  mSWBits.set(AMDILDeviceInfo::BarrierDetect);
  mHWBits.reset(AMDILDeviceInfo::LongOps);
  mSWBits.set(AMDILDeviceInfo::LongOps);
  mSWBits.set(AMDILDeviceInfo::LocalMem);
}

size_t AMDIL770Device::getWavefrontSize() const
{
  return AMDILDevice::WavefrontSize;
}

AMDIL710Device::AMDIL710Device(AMDILSubtarget *ST) : AMDIL7XXDevice(ST)
{
}

AMDIL710Device::~AMDIL710Device()
{
}

size_t AMDIL710Device::getWavefrontSize() const
{
  return AMDILDevice::QuarterWavefrontSize;
}
