# This file is part of BOINC.
# https://boinc.berkeley.edu
# Copyright (C) 2022 University of California
#
# BOINC is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation,
# either version 3 of the License, or (at your option) any later version.
#
# BOINC is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with BOINC.  If not, see <http://www.gnu.org/licenses/>.

import sys

def set_configure_ac(version):
    with open('configure.ac', 'r') as f:
        lines = f.readlines()
    with open('configure.ac', 'w') as f:
        for line in lines:
            if line.startswith('VBOXWRAPPER_RELEASE='):
                line = 'VBOXWRAPPER_RELEASE=%s\n' % (version)
            f.write(line)

def set_version_h(version):
    with open('version.h', 'r') as f:
        lines = f.readlines()
    with open('version.h', 'w') as f:
        for line in lines:
            if line.startswith('#define VBOXWRAPPER_RELEASE'):
                line = '#define VBOXWRAPPER_RELEASE %s\n' % (version)
            f.write(line)

def set_vcxproj(version):
    for vcxproj in ['win_build/vboxwrapper.vcxproj',
                    'win_build/vboxwrapper_vs2013.vcxproj',
                    'win_build/vboxwrapper_vs2019.vcxproj']:
        with open(vcxproj, 'r') as f:
            lines = f.readlines()
        with open(vcxproj, 'w') as f:
            for line in lines:
                if line.startswith('    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Debug|x64\'">vboxwrapper_'):
                    line = '    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Debug|x64\'">vboxwrapper_%s_windows_x86_64</TargetName>\n' % (version)
                elif line.startswith('    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Release|x64\'">vboxwrapper_'):
                    line = '    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Release|x64\'">vboxwrapper_%s_windows_x86_64</TargetName>\n' % (version)
                elif line.startswith('    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Debug|Win32\'">vboxwrapper_'):
                    line = '    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Debug|Win32\'">vboxwrapper_%s_windows_intelx86</TargetName>\n' % (version)
                elif line.startswith('    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Release|Win32\'">vboxwrapper_'):
                    line = '    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Release|Win32\'">vboxwrapper_%s_windows_intelx86</TargetName>\n' % (version)
                elif line.startswith('    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Debug|ARM64\'">vboxwrapper_'):
                    line = '    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Debug|ARM64\'">vboxwrapper_%s_windows_arm64</TargetName>\n' % (version)
                elif line.startswith('    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Release|ARM64\'">vboxwrapper_'):
                    line = '    <TargetName Condition="\'$(Configuration)|$(Platform)\'==\'Release|ARM64\'">vboxwrapper_%s_windows_arm64</TargetName>\n' % (version)
                f.write(line)

if (len(sys.argv) != 2):
    print('Usage: set-vboxwrapper-version.py VERSION')
    exit(1)

version = sys.argv[1]

print('Setting vboxwrapper version to %s...' % (version))

set_configure_ac(version)
set_version_h(version)
set_vcxproj(version)

print('Done.')
