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
#include "AMDILDevice.h"
#include "AMDILSubtarget.h"
using namespace llvm;
// Default implementation for all of the classes.
AMDILDevice::AMDILDevice(AMDILSubtarget *ST) : mSTM(ST)
{
  mHWBits.resize(AMDILDeviceInfo::MaxNumberCapabilities);
  mSWBits.resize(AMDILDeviceInfo::MaxNumberCapabilities);
  setCaps();
  mDeviceFlag = OCL_DEVICE_ALL;
}

AMDILDevice::~AMDILDevice()
{
    mHWBits.clear();
    mSWBits.clear();
}

size_t AMDILDevice::getMaxGDSSize() const
{
  return 0;
}

uint32_t 
AMDILDevice::getDeviceFlag() const
{
  return mDeviceFlag;
}

size_t AMDILDevice::getMaxNumCBs() const
{
  if (usesHardware(AMDILDeviceInfo::ConstantMem)) {
    return HW_MAX_NUM_CB;
  }

  return 0;
}

size_t AMDILDevice::getMaxCBSize() const
{
  if (usesHardware(AMDILDeviceInfo::ConstantMem)) {
    return MAX_CB_SIZE;
  }

  return 0;
}

size_t AMDILDevice::getMaxScratchSize() const
{
  return 65536;
}

uint32_t AMDILDevice::getStackAlignment() const
{
  return 16;
}

void AMDILDevice::setCaps()
{
  mSWBits.set(AMDILDeviceInfo::HalfOps);
  mSWBits.set(AMDILDeviceInfo::ByteOps);
  mSWBits.set(AMDILDeviceInfo::ShortOps);
  mSWBits.set(AMDILDeviceInfo::HW64BitDivMod);
  if (mSTM->isOverride(AMDILDeviceInfo::NoInline)) {
    mSWBits.set(AMDILDeviceInfo::NoInline);
  }
  if (mSTM->isOverride(AMDILDeviceInfo::MacroDB)) {
    mSWBits.set(AMDILDeviceInfo::MacroDB);
  }
  if (mSTM->isOverride(AMDILDeviceInfo::Debug)) {
    mSWBits.set(AMDILDeviceInfo::ConstantMem);
  } else {
    mHWBits.set(AMDILDeviceInfo::ConstantMem);
  }
  if (mSTM->isOverride(AMDILDeviceInfo::Debug)) {
    mSWBits.set(AMDILDeviceInfo::PrivateMem);
  } else {
    mHWBits.set(AMDILDeviceInfo::PrivateMem);
  }
  if (mSTM->isOverride(AMDILDeviceInfo::BarrierDetect)) {
    mSWBits.set(AMDILDeviceInfo::BarrierDetect);
  }
  mSWBits.set(AMDILDeviceInfo::ByteLDSOps);
  mSWBits.set(AMDILDeviceInfo::LongOps);
}

AMDILDeviceInfo::ExecutionMode
AMDILDevice::getExecutionMode(AMDILDeviceInfo::Caps Caps) const
{
  if (mHWBits[Caps]) {
    assert(!mSWBits[Caps] && "Cannot set both SW and HW caps");
    return AMDILDeviceInfo::Hardware;
  }

  if (mSWBits[Caps]) {
    assert(!mHWBits[Caps] && "Cannot set both SW and HW caps");
    return AMDILDeviceInfo::Software;
  }

  return AMDILDeviceInfo::Unsupported;

}

bool AMDILDevice::isSupported(AMDILDeviceInfo::Caps Mode) const
{
  return getExecutionMode(Mode) != AMDILDeviceInfo::Unsupported;
}

bool AMDILDevice::usesHardware(AMDILDeviceInfo::Caps Mode) const
{
  return getExecutionMode(Mode) == AMDILDeviceInfo::Hardware;
}

bool AMDILDevice::usesSoftware(AMDILDeviceInfo::Caps Mode) const
{
  return getExecutionMode(Mode) == AMDILDeviceInfo::Software;
}

std::string
AMDILDevice::getDataLayout() const
{
    return std::string("e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16"
      "-i32:32:32-i64:64:64-f32:32:32-f64:64:64-f80:32:32"
      "-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64"
      "-v96:128:128-v128:128:128-v192:256:256-v256:256:256"
      "-v512:512:512-v1024:1024:1024-v2048:2048:2048"
      "-n8:16:32:64");
}
