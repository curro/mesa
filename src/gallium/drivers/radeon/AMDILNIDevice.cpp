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
#include "AMDILNIDevice.h"
#include "AMDILEvergreenDevice.h"
#include "AMDILSubtarget.h"
using namespace llvm;

AMDILNIDevice::AMDILNIDevice(AMDILSubtarget *ST)
  : AMDILEvergreenDevice(ST)
{
  std::string name = ST->getDeviceName();
  if (name == "caicos") {
    mDeviceFlag = OCL_DEVICE_CAICOS;
  } else if (name == "turks") {
    mDeviceFlag = OCL_DEVICE_TURKS;
  } else if (name == "cayman") {
    mDeviceFlag = OCL_DEVICE_CAYMAN;
  } else {
    mDeviceFlag = OCL_DEVICE_BARTS;
  }
}
AMDILNIDevice::~AMDILNIDevice()
{
}

size_t
AMDILNIDevice::getMaxLDSSize() const
{
  if (usesHardware(AMDILDeviceInfo::LocalMem)) {
    return MAX_LDS_SIZE_900;
  } else {
    return 0;
  }
}

uint32_t
AMDILNIDevice::getGeneration() const
{
  return AMDILDeviceInfo::HD6XXX;
}


AMDILCaymanDevice::AMDILCaymanDevice(AMDILSubtarget *ST)
  : AMDILNIDevice(ST)
{
  setCaps();
}

AMDILCaymanDevice::~AMDILCaymanDevice()
{
}

void
AMDILCaymanDevice::setCaps()
{
  if (mSTM->isOverride(AMDILDeviceInfo::DoubleOps)) {
    mHWBits.set(AMDILDeviceInfo::DoubleOps);
    mHWBits.set(AMDILDeviceInfo::FMA);
  }
  mHWBits.set(AMDILDeviceInfo::Signed24BitOps);
  mSWBits.reset(AMDILDeviceInfo::Signed24BitOps);
  mSWBits.set(AMDILDeviceInfo::ArenaSegment);
}

