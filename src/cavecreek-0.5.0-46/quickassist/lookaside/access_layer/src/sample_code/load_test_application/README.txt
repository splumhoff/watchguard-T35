/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or 
 *   redistributing this file, you may do so under either license.
 * 
 *   GPL LICENSE SUMMARY
 * 
 *   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
 * 
 *   This program is free software; you can redistribute it and/or modify 
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 * 
 *   This program is distributed in the hope that it will be useful, but 
 *   WITHOUT ANY WARRANTY; without even the implied warranty of 
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 *   General Public License for more details.
 * 
 *   You should have received a copy of the GNU General Public License 
 *   along with this program; if not, write to the Free Software 
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution 
 *   in the file called LICENSE.GPL.
 * 
 *   Contact Information:
 *   Intel Corporation
 * 
 *   BSD LICENSE 
 * 
 *   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without 
 *   modification, are permitted provided that the following conditions 
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright 
 *       notice, this list of conditions and the following disclaimer in 
 *       the documentation and/or other materials provided with the 
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its 
 *       contributors may be used to endorse or promote products derived 
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * 
 *  version: SXXXX.L.0.5.0-46
 *
 *****************************************************************************/

===============================================================================
Overview
===============================================================================
The accel_load_test app reuses the QuickAssist Driver to execute four distinct
tests, that are replicated across the available logical Intel(r) QuickAssist:

    1.  Symmetric: AES-CBC-128 Cipher Test w/ 2k buffers
    2.  Symmetric: SHA512 Hash Test w/ 2k buffers
    3.  Asymmetric: RSA(1024 bit)
    4.  Compression: L3 Dynamic w/ 4k buffers(Compression supported devices only)

The application package includes a number of configuration files
(dh89xxcc_qa_dev<n>.conf/sxxxx_qa_dev0.conf) in the config_files directory.
These files define the number of QA instances to use on each platform. These
instances are mapped to specific accelerators and execution engines to ensure
an even load of the individual test threads across the underlying hardware and
allow for TDP measurements.

===============================================================================
Installing the Accel Load Test Application
===============================================================================
This README assumes that the user has followed the relevant documentation for
building and installing the acceleration software and the user space sample 
code.

This application includes a "setup_accel_load_test.sh" bash script for setting the
environment variables necessary to build the application.

This script assumes that the user is executing the commands from the installed 
path

ie. 

cd <INSTALL_PATH>/quickassist/lookaside/access_layer/src/sample_code/load_test_application/


Execute the script with the "source" command:
    source setup_load_test_env.sh

To build the application, execute the following commands from a bash shell:
    rm -rf ./build/ && make clean && make

The resulting accel_load_test application will be created in the 
./build/linux_2.6/user_space/ sub-directory.

Copy the corresponding configuration
files (dh89xxcc_qa_dev{n}.conf/sxxxx_qa_dev0.conf) from config_files/ to /etc/.
Configuration files for SKU2 of DH89XXCC are also included, where a single
accelerator is available on each device.


To ensure that the acceleration driver picks up the custom configuration, it is 
necessary to execute the following:
    $ICP_ROOT/build/adf_ctl down
    $ICP_ROOT/build/adf_ctl up
    insmod $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code/build/qaeMemDrv.ko

===============================================================================
Running the Application
===============================================================================
Upon each reboot of the OS please execute the following script to set the 
required environment variables:
    cd <INSTALL_PATH>/quickassist/lookaside/access_layer/src/sample_code/load_test_application/
    source setup_load_test_env.sh

The application is run without arguments:
    ./build/linux_2.6/user_space/accel_load_test_app

The application is designed to run over a number of hours, but it can be 
stopped by sending a SIGINT(Ctrl-c) signal.

To verify that operations are being executed on each accelerator, the device
statistics can be queried:

For example, for the first SKU3/SKU4 dh89xx device on the system:
     cat /proc/icp_dh89xxcc_dev0/qat0
     cat /proc/icp_dh89xxcc_dev0/qat1

or for SKU2 dh89xx device
    cat /proc/icp_dh89xxcc_dev0/qat0
    
or for sxxxx device:
    cat /proc/icp_sxxxx_dev0/qat0 
   

It is recommended to wait 2-3 minutes for after the "instances initialization
completed" message is printed before taking measurements, as the memory
allocation for the compression threads can delay the execution of 
operations and may skew measurement taken earlier.
As it may take some time for the application to initialize, it is 
recommended that the device statistics are queried first to see 
operations in-flight before taking measurements.

A sample output looks like the following:

Notes: 
1. The performance statistics are not printed after Ctrl-c. The are only 
printed if the application is left to run to completion.

2. Any performance statistics printed should not be taken out of context as
individual threads are not synchronised and therefore skew results. Some 
threads will return before others. If thread statistics are printed while 
measurements are being taken, it is advisable to restart the application to 
ensure the hardware is under sufficient load.

Sample Output (using dh89xx SKU4 device):
./build/linux_2.6/user_space/accel_load_test_app
Initializing user space "LoadTest" instances...
"LoadTest" instances initialization completed
Creating Crypto Tests across 12 logical instances
Creating Cipher(AES_CBC) thread:      instance   0, core  0, device  0, accelerator  0, engine  0
Creating Hash(Auth SHA512) thread:    instance   1, core  1, device  0, accelerator  0, engine  0
Creating RSA(mod 1024 Type 2) thread: instance   2, core  2, device  0, accelerator  0, engine  0
Creating Cipher(AES_CBC) thread:      instance   3, core  3, device  0, accelerator  0, engine  1
Creating Hash(Auth SHA512) thread:    instance   4, core  4, device  0, accelerator  0, engine  1
Creating RSA(mod 1024 Type 2) thread: instance   5, core  5, device  0, accelerator  0, engine  1
Creating Cipher(AES_CBC) thread:      instance   6, core 16, device  0, accelerator  1, engine  0
Creating Hash(Auth SHA512) thread:    instance   7, core 17, device  0, accelerator  1, engine  0
Creating RSA(mod 1024 Type 2) thread: instance   8, core 18, device  0, accelerator  1, engine  0
Creating Cipher(AES_CBC) thread:      instance   9, core 19, device  0, accelerator  1, engine  1
Creating Hash(Auth SHA512) thread:    instance  10, core 20, device  0, accelerator  1, engine  1
Creating RSA(mod 1024 Type 2) thread: instance  11, core 21, device  0, accelerator  1, engine  1
Creating Compression Tests across 2 logical instances
Creating Compression thread:          instance   0, core  6, device  0, accelerator  0
Creating Compression thread:          instance   1, core 22, device  0, accelerator  1
Starting test execution, press Ctl-c to exit

Sample Output (using sxxxx ):
./build/linux_2.6/user_space/accel_load_test_app
Initializing user space "LoadTest" instances...
"LoadTest" instances initialization completed
Creating Crypto Tests across 6 logical instances
Creating Cipher(AES_CBC) thread:      instance   0, core  0, device  0, accelerator  0, engine  0
Creating Hash(Auth SHA512) thread:    instance   1, core  1, device  0, accelerator  0, engine  0
Creating RSA(mod 1024 Type 2) thread: instance   2, core  2, device  0, accelerator  0, engine  0
Creating Cipher(AES_CBC) thread:      instance   3, core  3, device  0, accelerator  0, engine  1
Creating Hash(Auth SHA512) thread:    instance   4, core  4, device  0, accelerator  0, engine  1
Creating RSA(mod 1024 Type 2) thread: instance   5, core  5, device  0, accelerator  0, engine  1
Warning: No Data Compression Instances present
Starting test execution, press Ctl-c to exit

===============================================================================
Running the Ethernet Loop-back Test
===============================================================================

For TDP testing that involves the embedded Ethernet device(s) (if available), 
the following test can be run from the command line using "ethtool".
For each interface, run "ethtool -t <interface-name>". This test generally lasts
for 15 seconds and can be looped over using a shell script, for example, using 
bash:

    #for i in {1..300}
    #do
    #   ethtool -t <interface>
    #done


