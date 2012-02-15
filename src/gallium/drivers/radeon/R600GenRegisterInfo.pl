#
# Copyright 2011 Advanced Micro Devices, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
# Authors: Tom Stellard <thomas.stellard@amd.com>
#

use strict;
use warnings;

use AMDGPUConstants;

my $REPL_REG_COUNT = 100;
my $CREG_MAX = CONST_REG_COUNT - 1;

print <<STRING;

class AMDGPUReg <bits<16> value, string name> : Register<name> {
  field bits<16> Value;
  let Value = value;
  let Namespace = "AMDIL";
}

class AMDGPUInputReg <bits<16> value, string name, Register gprAlias> :
    AMDGPUReg<value, name> {

  let Aliases = [gprAlias];
}

STRING

my $i;

### CONSTANT REGS ###

my @creg_list;
for ($i = 0; $i < CONST_REG_COUNT; $i++) {
  print const_reg($i);
  $creg_list[$i] = "C$i";
}

print 'def R600_CReg_32 : RegisterClass <"AMDIL", [f32, i32], 32, (sequence "C%u", 0, ', CONST_REG_COUNT - 1, ")>;\n";

sub const_reg {
  my ($index) = @_;
  return sprintf(qq{def C%d : AMDGPUReg <%d, "C%d">;\n}, $index, $index, $index);
}

print <<STRING;

def ZERO : AMDILReg<871, "0.0">;
def HALF : AMDILReg<872, "0.5">;
def ONE : AMDILReg<873, "1.0">;
def NEG_HALF : AMDILReg<874, "-0.5">;
def NEG_ONE : AMDILReg<875, "-1.0">;
def PV_X : AMDILReg<876, "pv.x">;
def ALU_LITERAL_X : AMDILReg<877, "literal.x">;

def R600_Reg32 : RegisterClass <"AMDIL", [f32, i32], 32, (add
  (sequence "C%u", 0, $CREG_MAX),
  (sequence "R%u", 1, 128), ZERO, HALF, ONE, PV_X, ALU_LITERAL_X)>;

let Namespace = "AMDIL" in {
def sel_x : SubRegIndex;
def sel_y : SubRegIndex;
def sel_z : SubRegIndex;
def sel_w : SubRegIndex;
}

class AMDGPURegWithSubReg<string n, list<Register> subregs> : RegisterWithSubRegs<n, subregs> {
  let Namespace = "AMDIL";
  let SubRegIndices = [sel_x, sel_y, sel_z, sel_w];
}

STRING

### REPL REGS ###
my @repl_reg_list;

for (my $i = 0; $i < $REPL_REG_COUNT; $i++) {
  print repl_reg($i);
  $repl_reg_list[$i] = "REPL$i";
}

print 'def REPL : RegisterClass<"AMDIL", [v4f32], 128, (sequence "REPL%u", 0, ', $REPL_REG_COUNT - 1, ')';
print ">;\n\n";

sub repl_reg {
  my ($index) = @_;

  return sprintf(qq{def REPL%d : AMDGPURegWithSubReg<"R%d.xyzw", [R%d, R%d, R%d, R%d]>;\n},
    $index, $index, ($index * 4) + 1, ($index * 4) + 2, ($index * 4) + 3, ($index * 4) + 4);
}

print <<STRING;

def ADDR0 : AMDILReg<870, "addr0">;

def RELADDR : RegisterClass<"AMDIL", [i32], 32,
  (add ADDR0)
>;


def SPECIAL : RegisterClass<"AMDIL", [f32], 32, (add ZERO, HALF, ONE, NEG_HALF, NEG_ONE, PV_X)>; 

STRING
