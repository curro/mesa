//===- AMDILSubtarget.cpp - AMDIL Subtarget Information -------------------===//
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
// This file implements the AMD IL specific subclass of TargetSubtarget.
//
//===----------------------------------------------------------------------===//

#include "AMDILSubtarget.h"
#include "AMDIL.h"
#include "AMDILDevices.h"
#include "AMDILGlobalManager.h"
#include "AMDILKernelManager.h"
#include "AMDILUtilityFunctions.h"
#include "AMDILGenSubtarget.inc"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MC/SubtargetFeature.h"

using namespace llvm;

#define GET_SUBTARGETINFO_ENUM
#define GET_SUBTARGETINFO_CTOR
#define GET_SUBTARGETINFO_MC_DESC
#define GET_SUBTARGETINFO_TARGET_DESC
#include "AMDILGenSubtarget.inc"

AMDILSubtarget::AMDILSubtarget(llvm::StringRef TT, llvm::StringRef CPU, llvm::StringRef FS) : AMDILGenSubtargetInfo( TT, CPU, FS )
{
  memset(CapsOverride, 0, sizeof(*CapsOverride)
      * AMDILDeviceInfo::MaxNumberCapabilities);
  // Default card
  std::string GPU = "rv770";
  GPU = CPU;
  mIs64bit = false;
  mVersion = 0;
  SmallVector<StringRef, DEFAULT_VEC_SLOTS> Features;
  SplitString(FS, Features, ",");
  mDefaultSize[0] = 64;
  mDefaultSize[1] = 1;
  mDefaultSize[2] = 1;
  std::string newFeatures = "";
#if defined(_DEBUG) || defined(DEBUG)
  bool useTest = false;
#endif
  for (size_t x = 0; x < Features.size(); ++x) {
    if (Features[x].startswith("+mwgs")) {
      SmallVector<StringRef, DEFAULT_VEC_SLOTS> sizes;
      SplitString(Features[x], sizes, "-");
      size_t mDim = ::atoi(sizes[1].data());
      if (mDim > 3) {
        mDim = 3;
      }
      for (size_t y = 0; y < mDim; ++y) {
        mDefaultSize[y] = ::atoi(sizes[y+2].data());
      }
#if defined(_DEBUG) || defined(DEBUG)
    } else if (!Features[x].compare("test")) {
      useTest = true;
#endif
    } else if (Features[x].startswith("+cal")) {
      SmallVector<StringRef, DEFAULT_VEC_SLOTS> version;
      SplitString(Features[x], version, "=");
      mVersion = ::atoi(version[1].data());
    } else {
      GPU = CPU;
      if (x > 0) newFeatures += ',';
      newFeatures += Features[x];
    }
  }
  // If we don't have a version then set it to
  // -1 which enables everything. This is for
  // offline devices.
  if (!mVersion) {
    mVersion = (uint32_t)-1;
  }
  for (int x = 0; x < 3; ++x) {
    if (!mDefaultSize[x]) {
      mDefaultSize[x] = 1;
    }
  }
#if defined(_DEBUG) || defined(DEBUG)
  if (useTest) {
    GPU = "kauai";
  }
#endif
  ParseSubtargetFeatures(GPU, newFeatures);
#if defined(_DEBUG) || defined(DEBUG)
  if (useTest) {
    GPU = "test";
  }
#endif
  mDevName = GPU;
  mDevice = getDeviceFromName(mDevName, this, mIs64bit);
}
AMDILSubtarget::~AMDILSubtarget()
{
  delete mDevice;
}
bool
AMDILSubtarget::isOverride(AMDILDeviceInfo::Caps caps) const
{
  assert(caps < AMDILDeviceInfo::MaxNumberCapabilities &&
      "Caps index is out of bounds!");
  return CapsOverride[caps];
}
bool
AMDILSubtarget::is64bit() const 
{
  return mIs64bit;
}
bool
AMDILSubtarget::isTargetELF() const
{
  return false;
}
size_t
AMDILSubtarget::getDefaultSize(uint32_t dim) const
{
  if (dim > 3) {
    return 1;
  } else {
    return mDefaultSize[dim];
  }
}
uint32_t
AMDILSubtarget::calVersion() const
{
  return mVersion;
}

AMDILGlobalManager*
AMDILSubtarget::getGlobalManager() const
{
  return mGM;
}
void
AMDILSubtarget::setGlobalManager(AMDILGlobalManager *gm) const
{
  mGM = gm;
}

AMDILKernelManager*
AMDILSubtarget::getKernelManager() const
{
  return mKM;
}
void
AMDILSubtarget::setKernelManager(AMDILKernelManager *km) const
{
  mKM = km;
}
std::string
AMDILSubtarget::getDataLayout() const
{
    if (!mDevice) {
        return std::string("e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16"
                "-i32:32:32-i64:64:64-f32:32:32-f64:64:64-f80:32:32"
                "-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64"
                "-v96:128:128-v128:128:128-v192:256:256-v256:256:256"
                "-v512:512:512-v1024:1024:1024-v2048:2048:2048-a0:0:64");
    }
    return mDevice->getDataLayout();
}

std::string
AMDILSubtarget::getDeviceName() const
{
  return mDevName;
}
const AMDILDevice *
AMDILSubtarget::device() const
{
  return mDevice;
}
