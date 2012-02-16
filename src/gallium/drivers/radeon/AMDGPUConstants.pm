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

package AMDGPUConstants;

use base 'Exporter';

use constant CONST_REG_COUNT => 256;
use constant TEMP_REG_COUNT => 128;

our @EXPORT = ('TEMP_REG_COUNT', 'CONST_REG_COUNT', 'get_hw_index', 'get_chan_str');

sub get_hw_index {
  my ($index) = @_;
  return int($index / 4);
}

sub get_chan_str {
  my ($index) = @_;
  my $chan = $index % 4;
  if ($chan == 0 )  {
    return 'X';
  } elsif ($chan == 1) {
    return 'Y';
  } elsif ($chan == 2) {
    return 'Z';
  } elsif ($chan == 3) {
    return 'W';
  } else {
    die("Unknown chan value: $chan");
  }
}

1;
