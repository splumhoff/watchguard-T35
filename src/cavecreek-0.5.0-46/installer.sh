#!/bin/sh

###############################################################################
#
# This file is provided under a dual BSD/GPLv2 license.  When using or 
#   redistributing this file, you may do so under either license.
# 
#   GPL LICENSE SUMMARY
# 
#   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
# 
#   This program is free software; you can redistribute it and/or modify 
#   it under the terms of version 2 of the GNU General Public License as
#   published by the Free Software Foundation.
# 
#   This program is distributed in the hope that it will be useful, but 
#   WITHOUT ANY WARRANTY; without even the implied warranty of 
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
#   General Public License for more details.
# 
#   You should have received a copy of the GNU General Public License 
#   along with this program; if not, write to the Free Software 
#   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#   The full GNU General Public License is included in this distribution 
#   in the file called LICENSE.GPL.
# 
#   Contact Information:
#   Intel Corporation
# 
#   BSD LICENSE 
# 
#   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
#   All rights reserved.
# 
#   Redistribution and use in source and binary forms, with or without 
#   modification, are permitted provided that the following conditions 
#   are met:
# 
#     * Redistributions of source code must retain the above copyright 
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright 
#       notice, this list of conditions and the following disclaimer in 
#       the documentation and/or other materials provided with the 
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its 
#       contributors may be used to endorse or promote products derived 
#       from this software without specific prior written permission.
# 
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# 
#  version: SXXXX.L.0.5.0-46
#
###############################################################################

#Script Default Locations
#========================
INSTALL_DEFAULT="$PWD"
BUILD_OUTPUT_DEFAULT="build"
ICP_TOOLS_TARGET="accelcomp"
KERNEL_SOURCE_ROOT_DEFAULT="/usr/src/kernels/`\uname -r`"
OS="`\uname -m`"
ORIG_UMASK=`umask`
MODULE_DIR="/lib/modules/`\uname -r`/kernel/drivers"
OBJ_LIST="icp_qa_al.ko"
VF_OBJ_LIST="icp_qa_al_vf.ko"
BIN_LIST="mof_firmware.bin mmp_firmware.bin
          mof_firmware_sxxxx.bin mmp_firmware_sxxxx.bin"
VF_BIN_LIST="mmp_firmware.bin"
ERR_COUNT=0

#Copy Command not interactive
##################
alias cp='\cp'

#Command Line Arguments
#=========================
CL_INSTALL_OPTION_LIST=('a' 'h' 'ba' 'bs')
CL_INSTALL_OPTION=$1
CL_INSTALL_LOCATION=$2
CL_KERNEL_LOCATION=$3

if [ $CL_INSTALL_OPTION ]; then
for i in "${CL_INSTALL_OPTION_LIST[@]}"; do
    if [ $CL_INSTALL_OPTION = $i ]; then
        case=$CL_INSTALL_OPTION
    fi
done

fi

printCmdLineHelp()
{
echo -e "\n
The Installer script takes the following command line arguments:
Usage: ./installer.sh <*What to Build*> <*Where to Build*> <*Kernel Source*>

 <What to Build> Parameter:
ba - Build Acceleration
bs - Build Acceleration Sample Code Drivers

 <Where to Build> Parameter:
Set the build location, for example, /tmp or $PWD.

 <Kernel Source> Parameter:
Set the kernel source here, for example, /usr/src/kernels/3.4.11-yocto-standard.

 Example Usage:
./installer.sh ba /tmp /usr/src/kernels/3.4.11-yocto-standard.
\n
"
return
}

WelcomeMessage()
{
    if [ "$(id -u)" != "0" ]; then
        echo -e "\n\n\t==============================================="
        echo -e "\n\n\tERROR This script must be run as root"
        echo -e "\n\n\t===============================================\n\n"
        exit
    fi

   #Print Welcome Message
   #====================
   echo -e "\n\n\t===================================================="
   echo -e "\tWelcome to SXXXX Driver Interactive Installer"
   echo -e "\t====================================================\n\n"

}

CreateBuildFolders()
{
   if [ $CL_INSTALL_LOCATION ]; then
      echo -e "\n\n\tInstall Location set through command line :  $CL_INSTALL_LOCATION \n\n"
      ICP_ROOT=$CL_INSTALL_LOCATION
      ICP_BUILD_OUTPUT=$BUILD_OUTPUT_DEFAULT
      return
   fi

   #Read in Where to Untar/Build Package
   #================================
   echo -e "\tWhere Would you like to build the Sxxxx Driver?"
   echo -e "\tEnter to Accept Default [$INSTALL_DEFAULT]"
   read ICP_ROOT

   #Check if String is Null, If so use Default Value
   #================================================
   if [ -z $ICP_ROOT ]; then
   #echo "string is empty, apply Default"
   ICP_ROOT="$INSTALL_DEFAULT"
   #echo install dir : "$ICP_ROOT"
   else
   echo
        #echo "string is valid"
   #echo install dir : "$ICP_ROOT"
   fi

   #Create Directory
   #================
   if [ -e $ICP_ROOT ]; then
   echo
        #echo "This Directory Already Exists"
   #echo "OverWriting!!!"
   else
   echo
        #echo "Creating ICP_ROOT : $ICP_ROOT"
   fi

   mkdir -p "$ICP_ROOT"

   #Location of Build Output
   #================================
   echo -e "\tWhat would you like to call the Build Output Directory?"
   echo -e "\tEnter to Accept Default [$ICP_ROOT/$BUILD_OUTPUT_DEFAULT]"
   read ICP_BUILD_OUTPUT

   #Check if String is Null, If so use Default Value
   #================================================
   if [ -z $ICP_BUILD_OUTPUT ]; then
        #echo "string is empty, apply Default"
        ICP_BUILD_OUTPUT="$ICP_ROOT/$BUILD_OUTPUT_DEFAULT"
        #echo ICP_BUILD_OUTPUT  : "$ICP_BUILD_OUTPUT"
   else
        echo
        #echo "string is valid"
        #echo ICP_BUILD_OUTPUT : "$ICP_BUILD_OUTPUT"
   fi

   #Create Directory
   #================
   if [ -e $ICP_BUILD_OUTPUT ]; then
        echo
        #echo "This Directory Already Exists"
        #echo "OverWriting!!!"
   else
       echo
       #echo "Creating New Directory $ICP_BUILD_OUTPUT"
   fi

   #echo Creating ICP_BUILD_OUTPUT : "$ICP_BUILD_OUTPUT"
   mkdir -p "$ICP_BUILD_OUTPUT"
}

SetENV()
{
   export ICP_ROOT=$ICP_ROOT
   export ICP_BUILD_OUTPUT=$ICP_BUILD_OUTPUT
   export ICP_BUILDSYSTEM_PATH=$ICP_ROOT/quickassist/build_system/
   export ICP_TOOLS_TARGET=$ICP_TOOLS_TARGET
   export ICP_ENV_DIR=$ICP_ROOT/quickassist/build_system/build_files/env_files
   export ICP_NONBLOCKING_PARTIALS_PERFORM=1

}

SetKernel()
{
   if [ $CL_KERNEL_LOCATION ]; then
      echo -e "\n\n\tKernel Location set throug command line :  $CL_KERNEL_LOCATION \n\n"
      KERNEL_ROOT_SOURCE=$CL_KERNEL_LOCATION
      return
   fi

   #Location of Kernel Source
   #================================
   echo -e "\tWhere is the Kernel Source Located ?"
   echo -e "\tEnter to Accept Default [$KERNEL_SOURCE_ROOT_DEFAULT]"
   read KERNEL_SOURCE_ROOT

   #Check if String is Null, If so use Default Value
   #================================================
   if [ -z $KERNEL_SOURCE_ROOT ]; then
        #echo "string is empty, apply Default"
        KERNEL_SOURCE_ROOT="$KERNEL_SOURCE_ROOT_DEFAULT"
        #echo KERNEL_SOURCE_ROOT  : "$KERNEL_SOURCE_ROOT"
   else
        echo
        #echo "string is valid"
        #echo KERNEL_SOURCE_ROOT : "$KERNEL_SOURCE_ROOT"
   fi
}

BuildAccel()
{
    if [ -e $ICP_ROOT/quickassist/ ]; then
        echo "The quickassist directory has already been created"
        echo "Do not run the tar command for quickassist"
    else
        echo "The quickassist directory is not present, do the tar command for quickassist"
        tar -zxof $ICP_ROOT/SXXXX_*.L.*.tar.gz
    fi

    cd $ICP_ROOT/quickassist
    touch lookaside/firmware/icp_qat_ae.uof
    touch lookaside/firmware/icp_qat_pke.mof
    make clean
    if make; then
        cd $ICP_BUILD_OUTPUT
        ls -la
        #echo ""
    else
        echo -e "\n\t***Acceleration Build Failed***>"
        return
    fi
}

PrintAccDeviceInfo()
{
    status=1
    if [ -z $ICP_ROOT ]; then
        #echo "string is empty, apply Default"
        ICP_ROOT="$INSTALL_DEFAULT"
    fi

    qatDevices=`lspci -n | grep "8086" | egrep "1f18|0434" | sed -e 's/ /-/g'`
    numQatDevice=`lspci -n | grep "8086" | egrep -c "1f18|0434"`
    echo -e "\n\t****numQatDevice=$numQatDevice***\n"

    if [ $numQatDevice != "0" ]; then
        for qatDevice in $qatDevices ; do
            bdf=`echo $qatDevice | awk -F'-' '{print $1}'`
            echo -e "\tBDF=$bdf"
            did=`echo $qatDevice | awk -F'-' '{print $3}'`

            case $did in
            8086:1f18) echo -e "\t***Sxxxx device detected***\n"
                       status=0
                ;;

             8086:0434) rid=`echo $qatDevice | awk -F'(' '{print $2}'`
                case $rid in
                rev-2*) echo -e "\t***DH89xxCC C0 Stepping detected***\n"
                        status=0
                        ;;
                     *) echo -e "\t***Error: Unsupported DH89xxCC Stepping detected***\n"
                        ;;
                esac
                ;;
            *) echo -e "\t***Error: Invalid device detected***\n"
               ;;
            esac
        done
    else
        echo -e "\t***Error: No Acceleration Device Detected***\n"
    fi
    return $status
}

BuildAccelSample()
{
    if [ -e $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code ]; then
        echo "The quickassist directory has already been created"
        echo "Do not run the tar command for quickassist"
    else
        echo "The quickassist directory is not present, do the tar command for quickassist"
        tar -zxof $ICP_ROOT/SXXXX_*.L.*.tar.gz
    fi

    cd $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code
    touch $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code/performance/compression/calgary
    touch $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code/performance/compression/canterbury
    cp $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code/performance/compression/calgary /lib/firmware
    cp $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code/performance/compression/canterbury /lib/firmware
    make perf_all
    if make; then
        cd $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code/build
        ls -la
        #echo ""
    else
        echo -e "\n\t***Acceleration Sample Code Build Failed***>"
        return
    fi
}

InstallAccel()
{
    numDh89xxccDev=`lspci -n | grep "8086" | grep -c "0434"`
    PrintAccDeviceInfo
    if [ "$?" -ne "0" ]; then
        echo -e "\n\t***Aborting Installation***>\n"
        return
    fi

    if [ -e $ICP_ROOT/quickassist/ ]; then
        echo "The quickassist directory has already been created"
        echo "Do not run the tar command for quickassist"
    else
        echo "The quickassist directory is not present, do the tar command for quickassist"
        tar -zxof $ICP_ROOT/SXXXX_*.L.*.tar.gz
    fi

    cd $ICP_ROOT/quickassist
    make clean
    if (make 2>&1 | tee -a $ICP_ROOT/InstallerLog.txt); then

        cd $ICP_BUILD_OUTPUT

        echo "Copy QAT firmware to /lib/firmware"
        for bin_obj in $BIN_LIST;
            do
                install -D -m 640 $bin_obj /lib/firmware
            done

        # delete the existing kernel objects if they exist in /lib/modules
        echo "Copying QAT kernel object to $MODULE_DIR"
        for kern_obj in $OBJ_LIST;
            do
                install -D -m 640 $kern_obj $MODULE_DIR/$kern_obj
            done
        echo "Creating module.dep file for QAT released kernel object"
        echo "This will take a few moments"
        /sbin/depmod -a

        echo "Creating startup and kill scripts"
        install -D -m 750 qat_service /etc/init.d
        install -D -m 660 dh89xxcc_qa_dev0.conf /etc
        install -D -m 660 sxxxx_qa_dev0.conf /etc
        install -D -m 750 adf_ctl /etc/init.d
        install -D -m 750 gige_watchdog_service /etc/init.d
        install -D -m 750 icp_gige_watchdog /etc/init.d

        if [ $OS == "x86_64" ] && [ -d /lib64 ]; then
            echo "Copying icp_qa_al_s.so to /lib64"
            cp icp_qa_al_s.so /lib64
        else
            echo "Copying icp_qa_al_s.so to /lib"
            cp icp_qa_al_s.so /lib
        fi


        echo 'KERNEL=="icp_adf_ctl" MODE="0600"' > /etc/udev/rules.d/00-sxxxx_qa.rules
        echo 'KERNEL=="icp_dev[0-9]*" MODE="0600"' >> /etc/udev/rules.d/00-sxxxx_qa.rules
        echo 'KERNEL=="icp_dev_mem?" MODE="0600"' >> /etc/udev/rules.d/00-sxxxx_qa.rules
        echo 'KERNEL=="icp_adf_ctl" MODE="0600"' > /etc/udev/rules.d/00-dh89xxcc_qa.rules
        echo 'KERNEL=="icp_dev[0-9]*" MODE="0600"' >> /etc/udev/rules.d/00-dh89xxcc_qa.rules
        echo 'KERNEL=="icp_dev_mem?" MODE="0600"' >> /etc/udev/rules.d/00-dh89xxcc_qa.rules


        if [ -e /sbin/chkconfig ] ; then
            chkconfig --add qat_service
            if [ "$numDh89xxccDev" -ne "0" ]; then
                chkconfig --add gige_watchdog_service
            fi
        elif [ -e /usr/sbin/update-rc.d ]; then
            update-rc.d qat_service defaults
            if [ "$numDh89xxccDev" -ne "0" ]; then
                update-rc.d gige_watchdog_service defaults
            fi
        else
            echo "\n\t***Failed to add qat_service to start-up***>"
            return
        fi

        echo "Starting QAT service"
        /etc/init.d/qat_service start

        if [ "$numDh89xxccDev" -ne "0" ]; then
            echo "Starting GigE watchdog service"
            /etc/init.d/gige_watchdog_service start
        fi
    else
        echo -e "\n\t***Acceleration Installation Failed***>\n"
        return
    fi
    if [ `grep "Error" $ICP_ROOT/InstallerLog.txt | wc -l` != "0" ]; then
        echo -e "\n\t******** An error was detected in InstallerLog.txt file********\n"
        ERR_COUNT=1
    else
        echo -e "\n\t*** No error detected in InstallerLog.txt file ***\n"
        ERR_COUNT=0
    fi
}

UninstallAccel()
{
    if [ -e $MODULE_DIR/icp_qa_al.ko ]; then
        echo "Unloading QAT kernel object"
        echo "Removing startup scripts"
        # Check if one of the /etc/init.d script exist
        if [ -e /etc/init.d/qat_service ]; then
            if [ -e /etc/init.d/gige_watchdog_service ]; then
                /etc/init.d/gige_watchdog_service stop
            fi
            /etc/init.d/qat_service shutdown

            if [ -e /sbin/chkconfig ] ; then
                chkconfig --del gige_watchdog_service
                chkconfig --del qat_service
            elif [ -e /usr/sbin/update-rc.d ]; then
                update-rc.d -f gige_watchdog_service remove
                update-rc.d -f qat_service remove
            else
                echo "\n\t***Failed to remove qat_service from start-up***>"
                return
            fi
            /bin/rm -f /etc/init.d/gige_watchdog_service
            /bin/rm -f /etc/init.d/icp_gige_watchdog
            /bin/rm -f /etc/init.d/qat_service
            /bin/rm -f /etc/init.d/adf_ctl

        fi

        echo "Removing the QAT firmware"
        for bin_obj in $BIN_LIST
        do
            if [ -e /lib/firmware/$bin_obj ]; then
                /bin/rm -f /lib/firmware/$bin_obj
            fi
        done

        echo "Removing kernel objects from $MODULE_DIR"
        for kern_obj in $OBJ_LIST
        do
            if [ -e $MODULE_DIR/$kern_obj ]; then
                /bin/rm -f $MODULE_DIR/$kern_obj
                #incase the driver is a location that previous versions of the
                # installer placed it then attempt to remove it
                /bin/rm -f $MODULE_DIR/../../$kern_obj
            fi
        done
        echo "Removing device permissions rules"
        rm -f /etc/udev/rules.d/00-sxxxx_qa.rules
        rm -f /etc/udev/rules.d/00-dh89xxcc_qa.rules
        echo "Rebuilding the module.dep file, this will take a few moments"
        /sbin/depmod -a
        if [ $OS == "x86_64" ] && [ -d /lib64 ]; then
            rm -f /lib64/icp_qa_al_s.so
        else
            rm -f /lib/icp_qa_al_s.so
        fi

        if [ -e $MODULE_DIR/icp_qa_al_vf.ko ]; then
            for kern_obj in $VF_OBJ_LIST
            do
                if [ -e $MODULE_DIR/$kern_obj ]; then
                    /bin/rm -f $MODULE_DIR/$kern_obj
                fi
            done
        fi

        echo -e "\n\n\n\n\t***************************************"
        echo -e "\n\t*** Acceleration Uninstall Complete ***>"
        echo -e "\n\t***************************************\n\n\n\n\n"
    else
        echo -e "\n\n\t*** FAIL Acceleration Package Not Installed ***>\n\n\n"
        return
    fi

}

RemoveLogFile()
{
    echo -e "\n\n\n\t**********************************************"
    echo -e "\t* Removing log file in Build Location : $ICP_ROOT"
    rm -f $ICP_ROOT/InstallerLog.txt
    echo -e "\t************************************************\n\n\n\n\n"

}

PrintInstallOptions()
{
    echo -e "\n\n\n\t**********************************************"
    echo -e "\t* Build Location : $ICP_ROOT"
    echo -e "\t* Build Output Location : $ICP_BUILD_OUTPUT"
    if [ -n "$KERNEL_SOURCE_ROOT" ]; then
        echo -e "\t* Kernel Source Location : $KERNEL_SOURCE_ROOT"
    fi
    echo -e "\t************************************************\n\n\n\n\n"

}

PrintAccSampleInstallOptions()
{
    echo -e "\n\n\n\t**********************************************"
    echo -e "\t* Build Location : $ICP_ROOT"
    echo -e "\t* Build Location : $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code/build"
    if [ -n "$KERNEL_SOURCE_ROOT" ]; then
        echo -e "\t* Kernel Source Location : $KERNEL_SOURCE_ROOT"
    fi
    echo -e "\t************************************************\n\n\n\n\n"

}

LogFileHeader()
{
    echo -e "\n\n\n ****** `date`\t*********" 2>&1 > InstallerLog.txt
    echo -e " ************************************************\n" 2>&1 > InstallerLog.txt
}


MainViewGeneral()
{

    #Print Disclaimer
    #================
    WelcomeMessage
    #set permissions to create owner and group files only
        umask 7
    loop=0
    while [ $loop = 0 ];
    do


    if [ -z $CL_INSTALL_OPTION ]; then
       #Switch Statement to Select Installing Option
       #============================================
       echo -e "\t Please Select Install Option :"
       echo -e "\t ----------------------------"
       echo -e "\t 1  Build Acceleration"
       echo -e "\t 2  Install Acceleration"
       echo -e "\t 3  Show Acceleration Device Information"
       echo -e "\t 4  Build Acceleration Sample Code"
       echo -e "\t 5  Uninstall Acceleration"
       echo -e "\t 6  Exit"
       echo -e "\n"
       read case

    fi

    case $case in

        1 | ba)
        echo -e "\n\t****Building Acceleration****>\n\n"
        CreateBuildFolders
        SetENV
        RemoveLogFile
        PrintInstallOptions
        sleep 3
        BuildAccel 2>&1 | tee -a InstallerLog.txt
        echo -e "\n\n\n\t***Acceleration Build Complete***>\n\n"

        ;;

        2 | a)
        echo -e "\n\t****Installing Acceleration****>\n\n"
        CreateBuildFolders
        SetENV
        RemoveLogFile
        PrintInstallOptions
        sleep 3
        InstallAccel 2>&1
        echo -e "\n\n\n\t***Acceleration Installation Complete***>\n\n"

        ;;

        3 | a)
        echo -e "\n\t****Acceleration Information****>\n\n"
        SetENV
        PrintAccDeviceInfo

        ;;

        4 | bs)
        echo -e "\n\t****Building Acceleration Sample Code****>\n\n"
        CreateBuildFolders
        SetENV
        RemoveLogFile
        PrintAccSampleInstallOptions
        sleep 3
        BuildAccelSample 2>&1 | tee -a InstallerLog.txt
        echo -e "\n\n\n\t***Acceleration Sample Code Build Complete***>\n\n"

        ;;

        5)
        echo -e "\n\t****Uninstalling Acceleration****>\n\n"
        UninstallAccel
        echo -e "\n\n\n\t***Acceleration Uninstall Complete***>\n\n"

        ;;

        6)
        echo -e "\n\t****EXITING INSTALLER****>\n\n"

        exit
        ;;

        0)
        echo -e "\n\t****EXITING INSTALLER****>\n\n"

        exit
        ;;

        h | -h)
        #echo -e "\n\t****Installer Command Line Help****>\n\n"
        echo -e "\b\b\t\t================="
        echo -e "\t\tCommand Line Help"
        echo -e "\t\t================="
        printCmdLineHelp

        exit
        ;;

        *)
        echo -e "\n\t****Invalid Option****>"
        echo -e "\n\t****Please Choose Again****>\n\n"
        ;;
    esac

    #If Running with command line arguments exit after install
    #==================================================
    if [ $CL_INSTALL_OPTION ]; then
        exit
    fi
    #set umask back to original user setting
    umask $ORIG_UMASK
    done

}

MainViewGeneral

