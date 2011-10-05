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
#include "AMDILEvergreenDevice.h"
#include "AMDILEGAsmPrinter.h"
#include "AMDILIOExpansion.h"
#include "AMDILPointerManager.h"
using namespace llvm;

AMDILEvergreenDevice::AMDILEvergreenDevice(AMDILSubtarget *ST)
: AMDILDevice(ST) {
  setCaps();
  std::string name = ST->getDeviceName();
  if (name == "cedar") {
    mDeviceFlag = OCL_DEVICE_CEDAR;
  } else if (name == "redwood") {
    mDeviceFlag = OCL_DEVICE_REDWOOD;
  } else if (name == "cypress") {
    mDeviceFlag = OCL_DEVICE_CYPRESS;
  } else {
    mDeviceFlag = OCL_DEVICE_JUNIPER;
  }
}

AMDILEvergreenDevice::~AMDILEvergreenDevice() {
}

size_t AMDILEvergreenDevice::getMaxLDSSize() const {
  if (usesHardware(AMDILDeviceInfo::LocalMem)) {
    return MAX_LDS_SIZE_800;
  } else {
    return 0;
  }
}
size_t AMDILEvergreenDevice::getMaxGDSSize() const {
  if (usesHardware(AMDILDeviceInfo::RegionMem)) {
    return MAX_LDS_SIZE_800;
  } else {
    return 0;
  }
}
uint32_t AMDILEvergreenDevice::getMaxNumUAVs() const {
  return 12;
}

uint32_t AMDILEvergreenDevice::getResourceID(uint32_t id) const {
  switch(id) {
  default:
    assert(0 && "ID type passed in is unknown!");
    break;
  case CONSTANT_ID:
  case RAW_UAV_ID:
    if (mSTM->calVersion() >= CAL_VERSION_GLOBAL_RETURN_BUFFER) {
      return GLOBAL_RETURN_RAW_UAV_ID;
    } else {
      return DEFAULT_RAW_UAV_ID;
    }
  case GLOBAL_ID:
  case ARENA_UAV_ID:
    return DEFAULT_ARENA_UAV_ID;
  case LDS_ID:
    if (usesHardware(AMDILDeviceInfo::LocalMem)) {
      return DEFAULT_LDS_ID;
    } else {
      return DEFAULT_ARENA_UAV_ID;
    }
  case GDS_ID:
    if (usesHardware(AMDILDeviceInfo::RegionMem)) {
      return DEFAULT_GDS_ID;
    } else {
      return DEFAULT_ARENA_UAV_ID;
    }
  case SCRATCH_ID:
    if (usesHardware(AMDILDeviceInfo::PrivateMem)) {
      return DEFAULT_SCRATCH_ID;
    } else {
      return DEFAULT_ARENA_UAV_ID;
    }
  };
  return 0;
}

size_t AMDILEvergreenDevice::getWavefrontSize() const {
  return AMDILDevice::WavefrontSize;
}

uint32_t AMDILEvergreenDevice::getGeneration() const {
  return AMDILDeviceInfo::HD5XXX;
}

void AMDILEvergreenDevice::setCaps() {
  mSWBits.set(AMDILDeviceInfo::ArenaSegment);
  mHWBits.set(AMDILDeviceInfo::ArenaUAV);
  if (mSTM->calVersion() >= CAL_VERSION_SC_140) {
    mHWBits.set(AMDILDeviceInfo::HW64BitDivMod);
    mSWBits.reset(AMDILDeviceInfo::HW64BitDivMod);
  } 
  mSWBits.set(AMDILDeviceInfo::Signed24BitOps);
  if (mSTM->isOverride(AMDILDeviceInfo::ByteStores)) {
    mHWBits.set(AMDILDeviceInfo::ByteStores);
  }
  if (mSTM->isOverride(AMDILDeviceInfo::Debug)) {
    mSWBits.set(AMDILDeviceInfo::LocalMem);
    mSWBits.set(AMDILDeviceInfo::RegionMem);
  } else {
    mHWBits.set(AMDILDeviceInfo::LocalMem);
    mHWBits.set(AMDILDeviceInfo::RegionMem);
  }
  mHWBits.set(AMDILDeviceInfo::Images);
  if (mSTM->isOverride(AMDILDeviceInfo::NoAlias)) {
    mHWBits.set(AMDILDeviceInfo::NoAlias);
  }
  if (mSTM->calVersion() > CAL_VERSION_GLOBAL_RETURN_BUFFER) {
    mHWBits.set(AMDILDeviceInfo::CachedMem);
  }
  if (mSTM->isOverride(AMDILDeviceInfo::MultiUAV)) {
    mHWBits.set(AMDILDeviceInfo::MultiUAV);
  }
  if (mSTM->calVersion() > CAL_VERSION_SC_136) {
    mHWBits.set(AMDILDeviceInfo::ByteLDSOps);
    mSWBits.reset(AMDILDeviceInfo::ByteLDSOps);
    mHWBits.set(AMDILDeviceInfo::ArenaVectors);
  } else {
    mSWBits.set(AMDILDeviceInfo::ArenaVectors);
  }
  if (mSTM->calVersion() > CAL_VERSION_SC_137) {
    mHWBits.set(AMDILDeviceInfo::LongOps);
    mSWBits.reset(AMDILDeviceInfo::LongOps);
  }
  mHWBits.set(AMDILDeviceInfo::TmrReg);
}
FunctionPass* 
AMDILEvergreenDevice::getIOExpansion(
    TargetMachine& TM, CodeGenOpt::Level OptLevel) const
{
  return new AMDILEGIOExpansion(TM, OptLevel);
}

AsmPrinter*
AMDILEvergreenDevice::getAsmPrinter(AMDIL_ASM_PRINTER_ARGUMENTS) const
{
  return new AMDILEGAsmPrinter(ASM_PRINTER_ARGUMENTS);
}

FunctionPass*
AMDILEvergreenDevice::getPointerManager(
    TargetMachine& TM, CodeGenOpt::Level OptLevel) const
{
  return new AMDILEGPointerManager(TM, OptLevel);
}

AMDILCypressDevice::AMDILCypressDevice(AMDILSubtarget *ST)
  : AMDILEvergreenDevice(ST) {
  setCaps();
}

AMDILCypressDevice::~AMDILCypressDevice() {
}

void AMDILCypressDevice::setCaps() {
  if (mSTM->isOverride(AMDILDeviceInfo::DoubleOps)) {
    mHWBits.set(AMDILDeviceInfo::DoubleOps);
    mHWBits.set(AMDILDeviceInfo::FMA);
  }
}


AMDILCedarDevice::AMDILCedarDevice(AMDILSubtarget *ST)
  : AMDILEvergreenDevice(ST) {
  setCaps();
}

AMDILCedarDevice::~AMDILCedarDevice() {
}

void AMDILCedarDevice::setCaps() {
  mSWBits.set(AMDILDeviceInfo::FMA);
}

size_t AMDILCedarDevice::getWavefrontSize() const {
  return AMDILDevice::QuarterWavefrontSize;
}

AMDILRedwoodDevice::AMDILRedwoodDevice(AMDILSubtarget *ST)
  : AMDILEvergreenDevice(ST) {
  setCaps();
}

AMDILRedwoodDevice::~AMDILRedwoodDevice()
{
}

void AMDILRedwoodDevice::setCaps() {
  mSWBits.set(AMDILDeviceInfo::FMA);
}

size_t AMDILRedwoodDevice::getWavefrontSize() const {
  return AMDILDevice::HalfWavefrontSize;
}
