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
#ifndef _AMDILDEVICEINFO_H_
#define _AMDILDEVICEINFO_H_
#include <string>
namespace llvm
{
  class AMDILDevice;
  class AMDILSubtarget;
  namespace AMDILDeviceInfo
  {
    // Each Capabilities can be executed using a hardware instruction,
    // emulated with a sequence of software instructions, or not
    // supported at all.
    enum ExecutionMode {
      Unsupported = 0, // Unsupported feature on the card(Default value)
      Software, // This is the execution mode that is set if the
      // feature is emulated in software
      Hardware  // This execution mode is set if the feature exists
        // natively in hardware
    };

    // Any changes to this needs to have a corresponding update to the
    // twiki page GPUMetadataABI
    enum Caps {
      HalfOps          = 0x1,  // Half float is supported or not.
      DoubleOps        = 0x2,  // Double is supported or not.
      ByteOps          = 0x3,  // Byte(char) is support or not.
      ShortOps         = 0x4,  // Short is supported or not.
      LongOps          = 0x5,  // Long is supported or not.
      Images           = 0x6,  // Images are supported or not.
      ByteStores       = 0x7,  // ByteStores available(!HD4XXX).
      ConstantMem      = 0x8,  // Constant/CB memory.
      LocalMem         = 0x9,  // Local/LDS memory.
      PrivateMem       = 0xA,  // Scratch/Private/Stack memory.
      RegionMem        = 0xB,  // OCL GDS Memory Extension.
      FMA              = 0xC,  // Use HW FMA or SW FMA.
      ArenaSegment     = 0xD,  // Use for Arena UAV per pointer 12-1023.
      MultiUAV         = 0xE,  // Use for UAV per Pointer 0-7.
      Reserved0        = 0xF,  // ReservedFlag
      NoAlias          = 0x10, // Cached loads.
      Signed24BitOps   = 0x11, // Peephole Optimization.
      // Debug mode implies that no hardware features or optimizations
      // are performned and that all memory access go through a single
      // uav(Arena on HD5XXX/HD6XXX and Raw on HD4XXX).
      Debug            = 0x12, // Debug mode is enabled.
      CachedMem        = 0x13, // Cached mem is available or not.
      BarrierDetect    = 0x14, // Detect duplicate barriers.
      Reserved1        = 0x15, // Reserved flag
      ByteLDSOps       = 0x16, // Flag to specify if byte LDS ops are available.
      ArenaVectors     = 0x17, // Flag to specify if vector loads from arena work.
      TmrReg           = 0x18, // Flag to specify if Tmr register is supported.
      NoInline         = 0x19, // Flag to specify that no inlining should occur.
      MacroDB          = 0x1A, // Flag to specify that backend handles macrodb.
      HW64BitDivMod    = 0x1B, // Flag for backend to generate 64bit div/mod.
      ArenaUAV         = 0x1C, // Flag to specify that arena uav is supported.
      PrivateUAV       = 0x1D, // Flag to specify that private memory uses uav's.
      // If more capabilities are required, then
      // this number needs to be increased.
      // All capabilities must come before this
      // number.
      MaxNumberCapabilities = 0x20
    };
    // These have to be in order with the older generations
    // having the lower number enumerations.
    enum Generation {
      HD4XXX = 0, // 7XX based devices.
      HD5XXX, // Evergreen based devices.
      HD6XXX, // NI/Evergreen+ based devices.
      HDTEST, // Experimental feature testing device.
      HDNUMGEN
    };


  } // namespace AMDILDeviceInfo
  llvm::AMDILDevice*
    getDeviceFromName(const std::string &name, llvm::AMDILSubtarget *ptr, bool is64bit = false, bool is64on32bit = false);
} // namespace llvm
#endif // _AMDILDEVICEINFO_H_