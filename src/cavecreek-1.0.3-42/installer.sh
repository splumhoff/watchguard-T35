###############################################################################
#
#   BSD LICENSE
# 
#   Copyright(c) 2007-2016 Intel Corporation. All rights reserved.
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
#  version: QAT1.7.Upstream.L.1.0.3-42
#
###############################################################################

#Script Default Locations
#========================
INSTALL_DEFAULT="$PWD"
BUILD_OUTPUT_DEFAULT="build"
ICP_TOOLS_TARGET="accelcomp"
KERNEL_SOURCE_ROOT_DEFAULT="/lib/modules/`\uname -r`/build/"
KERNEL_SOURCE_ROOT_UPDATE="/lib/modules/`\uname -r`/updates/"
OS="`\uname -m`"
ORIG_UMASK=`umask`
BIN_LIST="qat_895xcc.bin qat_895xcc_mmp.bin qat_c3xxx.bin qat_c3xxx_a0.bin qat_c3xxx_mmp.bin qat_c62x.bin qat_c62x_a0.bin qat_c62x_mmp.bin qat_mmp.bin qat_d15xx.bin qat_d15xx_mmp.bin"
ERR_COUNT=0
KO_INTEL_QAT="drivers/crypto/qat/qat_common/intel_qat.ko"
KO_QAT_DH895XCC="drivers/crypto/qat/qat_dh895xcc/qat_dh895xcc.ko"
KO_QAT_DH895XCCVF="drivers/crypto/qat/qat_dh895xccvf/qat_dh895xccvf.ko"
KO_QAT_C62X="drivers/crypto/qat/qat_c62x/qat_c62x.ko"
KO_QAT_C62XVF="drivers/crypto/qat/qat_c62xvf/qat_c62xvf.ko"
KO_QAT_C3XXX="drivers/crypto/qat/qat_c3xxx/qat_c3xxx.ko"
KO_QAT_C3XXXVF="drivers/crypto/qat/qat_c3xxxvf/qat_c3xxxvf.ko"
KO_QAT_D15XX="drivers/crypto/qat/qat_d15xx/qat_d15xx.ko"
KO_QAT_D15XXVF="drivers/crypto/qat/qat_d15xxvf/qat_d15xxvf.ko"
QAT_DH895XCC_NUM_VFS=32
QAT_DHC62X_NUM_VFS=16
QAT_DHC3XXX_NUM_VFS=16
QAT_DHD15XX_NUM_VFS=16
INTEL_VENDORID="8086"
DH895_DEVICE_NUMBER="0435"
DH895_DEVICE_NUMBER_VM="0443"
C62X_DEVICE_NUMBER="37c8"
C62X_DEVICE_NUMBER_VM="37c9"
C3XXX_DEVICE_NUMBER="19e2"
C3XXX_DEVICE_NUMBER_VM="19e3"
D15XX_DEVICE_NUMBER="6f54"
D15XX_DEVICE_NUMBER_VM="6f55"
InstallerOperationStatus=0
#Copy Command not interactive
##################
alias cp='\cp'

#Command Line Arguments
#=========================
CL_INSTALL_OPTION_LIST=('a' 'u' 's' 'h' 'ba' 'bs' 'ha' 'ga')
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
Usage: ./installer <*What to Build*> <*Where to Build*> <*Kernel Source*>

 <What to Build> Parameter:
a  - Install Acceleration
ba - Build Acceleration
ha - Install Acceleration on Host (SR-IOV)
ga - Install Acceleration on Guest (SR-IOV)
bs - Build Acceleration Sample Code
u  - Uninstall Acceleration

 <Where to Build> Parameter:
Set the build location, for example, /tmp or $PWD.

 <Kernel Source> Parameter:
Set the kernel source here, for example, /lib/modules/linux-3.1.0-7/source/

 Note: The <Kernel Source> parameter is only required when patching the PCH drivers; for
the options Acceleration & Embedded and Embedded.

 Example Usage:
./installer a /tmp /lib/modules/linux-3.1.0-7/source/
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
   echo -e "\n\n\t==============================================="
   echo -e "\tWelcome to Intel(R) QuickAssist Interactive Installer"
   echo -e "\t===============================================\n\n"

}



CreateBuildFolders()
{
   if [ $CL_INSTALL_LOCATION ]; then
      echo -e "\n\n\tInstall Location set through command line :  $CL_INSTALL_LOCATION \n\n"
      ICP_ROOT=$CL_INSTALL_LOCATION
      ICP_BUILD_OUTPUT="$ICP_ROOT/$BUILD_OUTPUT_DEFAULT"
      KERNEL_SOURCE_ROOT=$CL_KERNEL_LOCATION
      return
   fi
   #
   #Read in Where to Untar/Build Package
   #================================
   echo -e "\tWhere Would you like to  Build the QAT Package?"
   echo -e "\tEnter to Accept Default [$INSTALL_DEFAULT]"
   read ICP_ROOT

   #Check if String is Null, If so use Default Value
   #================================================
   if [ -z $ICP_ROOT ]; then
       ICP_ROOT="$INSTALL_DEFAULT"
   fi

   #Create Directory
   #================
   if [ ! -e $ICP_ROOT ]; then
        mkdir -p "$ICP_ROOT"
   fi

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
   fi

   #Create Directory
   #================
   if [ ! -e $ICP_BUILD_OUTPUT ]; then
        mkdir -p "$ICP_BUILD_OUTPUT"
   fi

   #Location of Kernel Source
   #================================
   echo -e "\tEnter Kernel Source dir"
   echo -e "\tEnter to Accept Default [$KERNEL_SOURCE_ROOT_DEFAULT]"
   read KERNEL_SOURCE_ROOT

   #Check if String is Null, If so use Default Value
   #================================================
   if [ -z $KERNEL_SOURCE_ROOT ]; then
        #echo "string is empty, apply Default"
        KERNEL_SOURCE_ROOT="$KERNEL_SOURCE_ROOT_DEFAULT"
   fi


}

CheckStackProtection()
{
   echo -e "#include <stdio.h>" > "stack_protection_check.c"
   echo -e "int main()" >> "stack_protection_check.c"
   echo -e "{" >> "stack_protection_check.c"
   echo -e "    volatile char buffer[64];" >> "stack_protection_check.c"
   echo -e "    printf(\"Stack protection works\");" >> "stack_protection_check.c"
   echo -e "    return 0;" >> "stack_protection_check.c"
   echo -e "}" >> "stack_protection_check.c"
   # Try compiling with stack protector
   gcc -o stack_protection_check -fstack-protector-all stack_protection_check.c 2>&1 |grep Success;(exit ${PIPESTATUS[0]})
   if [ $? -eq 1 ]; then
       # Enable ICP_DEFENSES_SP only in case the previous compilation worked succesfully
       export ICP_DEFENSES_STACK_PROTECTION=n
       echo -e "\n\t***WARNING: Compiling without stack protection cause GCC version does not support it***>"
   fi
   /bin/rm -f stack_protection_check.c
}

SetENV()
{
   export ICP_ROOT=$ICP_ROOT
   export ICP_BUILD_OUTPUT=$ICP_BUILD_OUTPUT
   export ICP_BUILDSYSTEM_PATH=$ICP_ROOT/quickassist/build_system/
   export ICP_TOOLS_TARGET=$ICP_TOOLS_TARGET
   export ICP_ENV_DIR=$ICP_ROOT/quickassist/build_system/build_files/env_files
   export LD_LIBRARY_PATH=$ICP_ROOT/build
   export KERNEL_SOURCE_ROOT=$KERNEL_SOURCE_ROOT
   export KERNEL_SOURCE_DIR=$ICP_ROOT/quickassist/qat/
   export ICP_DC_DYN_NOT_SUPPORTED=1
   export SC_EPOLL_DISABLED=0
   export WITH_UPSTREAM=1
   export WITH_CMDRV=1
}

BuildAccel()
{

    cd $ICP_ROOT/quickassist/qat
#    make clean -C $KERNEL_SOURCE_ROOT M=$ICP_ROOT/quickassist/drivers/crypto/qat
#    make -j4 -C $KERNEL_SOURCE_ROOT M=$ICP_ROOT/quickassist/drivers/crypto/qat
    make KDIR=$KERNEL_SOURCE_ROOT clean
    make KDIR=$KERNEL_SOURCE_ROOT modules
    if [ "$?" -eq "0" ]; then
        echo -e "\n\t***Acceleration Build Complete***>"
    else
        echo -e "\n\t***Acceleration Build Failed***>"
        return 1
    fi

    cd $ICP_ROOT/quickassist
    if make; then
        cd $ICP_BUILD_OUTPUT
        ls -la
    else
        echo -e "\n\t***SAL Build Failed***>"
        return 1
    fi

    cd $ICP_ROOT/quickassist/utilities/adf_ctl
    if make; then
        cp $ICP_ROOT/quickassist/utilities/adf_ctl/adf_ctl $ICP_BUILD_OUTPUT
        cp $ICP_ROOT/quickassist/utilities/adf_ctl/conf_files/*.conf* $ICP_BUILD_OUTPUT
    else
        echo -e "\n\t***adf_ctl Build Failed***>"
        return 1
    fi
    cp $ICP_ROOT/quickassist/build_system/build_files/qat_service $ICP_BUILD_OUTPUT
    chmod 750 $ICP_BUILD_OUTPUT/qat_service
    cp $ICP_ROOT/quickassist/qat/fw/* $ICP_BUILD_OUTPUT
    cd $ICP_ROOT/quickassist/qat
    cp $KO_QAT_DH895XCC $KO_QAT_DH895XCCVF $KO_INTEL_QAT $KO_QAT_C62X $KO_QAT_C62XVF $KO_QAT_C3XXX $KO_QAT_C3XXXVF $KO_QAT_D15XX $KO_QAT_D15XXVF $ICP_BUILD_OUTPUT

}

# set pci values for specified device
# parameter 1 is the device number
# parameter 2 is the selected line (1-based)
SetPciVals()
{
    bdf=$(lspci -d:$1 | awk '{print $1}' | sed -n ''$2'p')
    # BDF format is <busNum>:<deviceNum>:<functionNum>
    busNum=$(echo $bdf | awk -F: '{print $1}')
    deviceNum=$(echo $bdf | awk -F: '{print $2}' | awk -F. '{print $1}')
    functionNum=$(echo $bdf | awk -F: '{print $2}' | awk -F. '{print $2}')
}

# Display the Stepping info for one dev
# parameter 1 is the device number
# parameter 2 is the selected line (1-based)
# parameter 3, print BDF if 'yes'
# set global failed flag on error
DetectStepping()
{
    SetPciVals $1 $2
    if [ "yes" == "$3" ]; then
        echo -e "\n\tBDF=$bdf"
    fi

    # vars set in SetPciVals above
    stepping=$(od -tx1 -Ax -j8 -N1 /proc/bus/pci/$busNum/$deviceNum.$functionNum | awk '{print $2}' | sed '/^$/d')

    if [ $1 == $DH895_DEVICE_NUMBER -o $1 == $DH895_DEVICE_NUMBER_VM ]; then
        case $stepping in
            00)
                echo -e "\t***DH895x Stepping A0 detected***>"
                ;;

            *)
                echo -e "\t***Error: Invalid Dh895x Stepping detected***>"
                failed=1
                ;;
        esac
    fi
    if [ $1 == $C62X_DEVICE_NUMBER -o $1 == $C62X_DEVICE_NUMBER_VM ]; then
        case $stepping in
           00)
                echo -e "\t***C62X Stepping A0 detected***>"
                ;;
           02)
                echo -e "\t***C62X Stepping B0 detected***>"
                ;;
           03)
                echo -e "\t***C62X Stepping B1 detected***>"
                ;;
           04)
                echo -e "\t***C62X Stepping B2 detected***>"
                ;;
           *)
                echo -e "\t***Error: Invalid C62X Stepping detected***>"
                failed=1
                ;;
        esac
    fi
    if [ $1 == $C3XXX_DEVICE_NUMBER -o $1 == $C3XXX_DEVICE_NUMBER_VM ]; then
            case $stepping in
               00)
                    echo -e "\t***C3xxx Stepping A0 detected***>"
                    ;;
               10)
                    echo -e "\t***C3xxx Stepping B0 detected***>"
                    ;;
               11)
                    echo -e "\t***C3xxx Stepping B1 detected***>"
                    ;;
               *)
                    echo -e "\t***Error: Invalid C3xxx Stepping detected***>"
                    failed=1
                    ;;
            esac
    fi

    if [ $1 == $D15XX_DEVICE_NUMBER -o $1 == $D15XX_DEVICE_NUMBER_VM  ]; then
        case $stepping in
           00)
                echo -e "\t***D15XX Stepping A0 detected***>"
                ;;
           02)
                echo -e "\t***D15XX Stepping B0 detected***>"
                ;;
           *)
                echo -e "\t***Error: Invalid D15XX Stepping detected***>"
                failed=1
                ;;
        esac
    fi
}

# local func to set some SKU values
# requires that SetPciVals be called previously
SetSkuVals()
{
    dhSkubit=$(od -tx1 -Ax -j64 -N4 /proc/bus/pci/$busNum/$deviceNum.$functionNum | awk '{print $4}' | sed '/^$/d')
    dhSkubit=$(echo $((16#${dhSkubit})))
    dhSku=$(echo $[${dhSkubit}&0x30])
}

# display SKU specific information
# parameter 1 is the device number
# parameter 2 is the selected line (1-based)
# set global failed flag on error
DetectSKU()
{
    SetSkuVals
    # device numbers displayed will be 0-based however
    local lin=$2
    dev=$((lin-1))

    # assume different for each dev ID
    if [ $1 == $C3XXX_DEVICE_NUMBER -o $1 == $C3XXX_DEVICE_NUMBER_VM -o $1 == $C62X_DEVICE_NUMBER -o $1 == $C62X_DEVICE_NUMBER_VM -o $1 == $D15XX_DEVICE_NUMBER -o $1 == $D15XX_DEVICE_NUMBER_VM -o  $1 == $DH895_DEVICE_NUMBER -o $1 == $DH895_DEVICE_NUMBER_VM ]; then
        case $dhSku in

            0)
                echo -e "\tdevice $dev is SKU1\n"
                ;;

            16)
                echo -e "\tdevice $dev is SKU2\n"
                ;;

            32)
                echo -e "\tdevice $dev is SKU3\n"
                ;;

            48)
                echo -e "\tdevice $dev is SKU4\n"
                ;;

            *)
                echo -e "\n\t***Error: Invalid SKU detected***>\n"
                failed=1
                ;;
        esac
    else
        echo -e "\t*** Invalid Device ***>\n"
    fi
}

# print an individual devices info
# parameter 1 is the device number
# parameter 2 is the selected line (1-based)
# set global failed flag on error
PrintSingleDevInfo()
{
    DetectStepping $1 $lin yes
    if [ $failed -eq 0 ]; then
        DetectSKU $1 $lin
    fi
}

# List the device info for one device type
# parameter 1 is the device number
# parameter 2 is the dev count
# listing stops if an error occurs
PrintDevInfo()
{
    lin=1
    while [ $lin -le $2 ]
    do
        PrintSingleDevInfo $1 $lin
        if [ $failed -ne 0 ]; then
            break
        fi
        ((lin++))
    done
}

# Display device info
# optional parameter 0 - Normal, 1, Virtual Host, 2 - Virtual Guest
PrintAccDeviceInfo()
{
    failed=0

    if [ -z $ICP_ROOT ]; then
        #echo "string is empty, apply Default"
        ICP_ROOT="$INSTALL_DEFAULT"
    fi

    if [ -z "$1" -o "$1" != "2" ]; then
        if [ "$ICP_DEV_TYPE_QAT16" == "QAT1.6" ];then
            echo "Number of Dh895x devices (Physical): $numDh895xDevicesP"
            PrintDevInfo $DH895_DEVICE_NUMBER $numDh895xDevicesP
        fi
        if [ "$ICP_DEV_TYPE_QAT17" == "QAT1.7" ];then
            echo "Number of C62x devices (Physical): $numC62xDevicesP"
            PrintDevInfo $C62X_DEVICE_NUMBER $numC62xDevicesP
        fi
        if [ "$ICP_DEV_TYPE_QAT171" == "QAT1.71" ];then
            echo "Number of D15xx devices (Physical): $numD15xxDevicesP"
            PrintDevInfo $D15XX_DEVICE_NUMBER $numD15xxDevicesP
        fi
        if [ "$ICP_DEV_TYPE_nQAT17" == "nQAT1.7" ];then
            echo "Number of C3xxx devices (Physical): $numC3xxxDevicesP"
            PrintDevInfo $C3XXX_DEVICE_NUMBER $numC3xxxDevicesP
        fi
    else
        if [ "$ICP_DEV_TYPE_QAT16" == "QAT1.6" ];then
            echo "Number of Dh895x devices ( Virtual): $numDh895xDevicesV"
            PrintDevInfo $DH895_DEVICE_NUMBER_VM $numDh895xDevicesV
        fi
        if [ "$ICP_DEV_TYPE_QAT17" == "QAT1.7" ];then
            echo "Number of C62x devices ( Virtual): $numC62xDevicesV"
            PrintDevInfo $C62X_DEVICE_NUMBER_VM $numC62xDevicesV
        fi
        if [ "$ICP_DEV_TYPE_QAT171" == "QAT1.71" ];then
            echo "Number of D15xx devices ( Virtual): $numD15xxDevicesV"
            PrintDevInfo $D15XX_DEVICE_NUMBER_VM $numD15xxDevicesV
        fi
        if [ "$ICP_DEV_TYPE_nQAT17" == "nQAT1.7" ];then
            echo "Number of C3xxx devices ( Virtual): $numC3xxxDevicesV"
            PrintDevInfo $C3XXX_DEVICE_NUMBER_VM $numC3xxxDevicesV
        fi
    fi
}

BuildAccelSample()
{
    PrintAccDeviceInfo
    if [ $failed -ne 0 ]; then
        echo -e "\n\t***Aborting Installation***>\n"
        return 1
    fi

    cd $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code
    if [ -d /lib/firmware ]; then
        echo "dir /lib/firmware exists"
    else
        mkdir /lib/firmware
        echo "dir /lib/firmware created"
    fi
    cp $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code/performance/compression/calgary /lib/firmware
    cp $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code/performance/compression/calgary32 /lib/firmware
    cp $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code/performance/compression/canterbury /lib/firmware
    make perf_user
    if [ "$?" -eq "0" ]; then
        cp $ICP_ROOT/quickassist/lookaside/access_layer/src/sample_code/performance/build/linux_2.6/user_space/cpa_sample_code $ICP_BUILD_OUTPUT
    else
        echo -e "\n\t***Acceleration Sample Code Build Failed***>"
        return 1
    fi
}


InstallAccel()
{
    PrintAccDeviceInfo
    if [ $failed -ne 0 ]; then
        echo -e "\n\t***Aborting Installation***>\n"
        return 1
    fi

    if [ -d /lib/firmware ]; then
        echo "/lib/firmware exists"
    else
        echo "/lib/firmware not exists, create it before installing"
        mkdir /lib/firmware
    fi

    if [ -d /lib/firmware/qat_fw ]; then
        echo "/lib/firmware/qat_fw exists"
    else
        echo "/lib/firmware/qat_fw not exists, create it before installing"
        mkdir /lib/firmware/qat_fw
    fi

    echo "Removing the existing QAT firmware files..."
    for bin_obj in $BIN_LIST
    do
        if [ -e /lib/firmware/$bin_obj ]; then
            if [ -e /lib/firmware/qat_fw/$bin_obj ]; then
                /bin/rm -f /lib/firmware/$bin_obj
            else
                /bin/mv /lib/firmware/$bin_obj /lib/firmware/qat_fw/$bin_obj
            fi
        fi
    done
   
    BuildAccel
    if [ "$?" -ne "0" ];then
        echo -e "\n\t***Acceleration Build Failed***>"
        return 1
    fi

    cd $ICP_ROOT/quickassist/qat
    make KDIR=$KERNEL_SOURCE_ROOT clean
    make KDIR=$KERNEL_SOURCE_ROOT modules_install
    if [ "$?" -ne "0" ]; then
        echo -e "\n\t***Acceleration Install Failed***>"
        return 1
    fi

    for bin_obj in $BIN_LIST;
    do
        if [ $bin_obj != "qat_mmp.bin" ]; then
            echo "Copying $bin_obj to /lib/firmware/"
            install -D -m 640 $ICP_BUILD_OUTPUT/$bin_obj /lib/firmware/
        fi
    done
    
    /bin/rm -f /etc/dh895xcc*.conf
    /bin/rm -f /etc/c6xx*.conf
    /bin/rm -f /etc/c3xxx*.conf

    if [ $1 != "2" ]; then
        # Host installation -- We need the PF config files.
        for (( dev=0; dev<$numDh895xDevicesP; dev++ ))
        do
	    if [ $1 = "1" ]; then
                install -D -m 640 $ICP_BUILD_OUTPUT/dh895xccpf_dev0.conf /etc/dh895xcc_dev${dev}.conf
	    else
                install -D -m 640 $ICP_BUILD_OUTPUT/dh895xcc_dev0.conf /etc/dh895xcc_dev${dev}.conf
	    fi
        done
        for (( dev=0; dev<$numC62xDevicesP; dev++ ))
        do
	    if [ $1 = "1" ]; then
                install -D -m 640 $ICP_BUILD_OUTPUT/c6xxpf_dev0.conf /etc/c6xx_dev"$dev".conf
	    else
                install -D -m 640 $ICP_BUILD_OUTPUT/c6xx_dev"$((${dev}%3))".conf /etc/c6xx_dev"$dev".conf
	    fi
        done
        for (( dev=0; dev<$numD15xxDevicesP; dev++ ))
        do
	    if [ $1 = "1" ]; then
                install -D -m 640 $ICP_BUILD_OUTPUT/d15xxpf_dev0.conf /etc/d15xx_dev"$dev".conf
	    else
                install -D -m 640 $ICP_BUILD_OUTPUT/d15xx_dev"$((${dev}%3))".conf /etc/d15xx_dev"$dev".conf
	    fi
        done
        for (( dev=0; dev<$numC3xxxDevicesP; dev++ ))
        do
	    if [ $1 = "1" ]; then
                install -D -m 640 $ICP_BUILD_OUTPUT/c3xxxpf_dev0.conf /etc/c3xxx_dev${dev}.conf
	    else
                install -D -m 640 $ICP_BUILD_OUTPUT/c3xxx_dev0.conf /etc/c3xxx_dev${dev}.conf
	    fi
        done
    fi
    if [ $1 = "1" ]; then
        # SRIOV on the host. Install config file for each possible VF.
        for (( dev=0; dev<$numDh895xDevicesP; dev++ ))
        do
            for (( vf_dev = 0; vf_dev<QAT_DH895XCC_NUM_VFS; vf_dev++ ))
            do
                vf_dev_num=`echo $dev \\* $QAT_DH895XCC_NUM_VFS + $vf_dev | bc`
                install -D -m 640 $ICP_BUILD_OUTPUT/dh895xccvf_dev0.conf.vm \
                        /etc/dh895xccvf_dev${vf_dev_num}.conf
            done
        done
        for (( dev=0; dev<$numC62xDevicesP; dev++ ))
        do
	    for (( vf_dev = 0; vf_dev<QAT_DHC62X_NUM_VFS; vf_dev++ ))
            do
                vf_dev_num=`echo $dev \\* $QAT_DHC62X_NUM_VFS + $vf_dev | bc`
                install -D -m 640 $ICP_BUILD_OUTPUT/c6xxvf_dev0.conf.vm \
                        /etc/c6xxvf_dev${vf_dev_num}.conf
            done
        done
        for (( dev=0; dev<$numD15xxDevicesP; dev++ ))
        do
	    for (( vf_dev = 0; vf_dev<QAT_DHD15XX_NUM_VFS; vf_dev++ ))
            do
                vf_dev_num=`echo $dev \\* $QAT_DHD15XX_NUM_VFS + $vf_dev | bc`
                install -D -m 640 $ICP_BUILD_OUTPUT/d15xxvf_dev0.conf.vm \
                        /etc/d15xxvf_dev${vf_dev_num}.conf
            done
        done
        for (( dev=0; dev<$numC3xxxDevicesP; dev++ ))
        do
            for (( vf_dev = 0; vf_dev<QAT_DHC3XXX_NUM_VFS; vf_dev++ ))
            do
                vf_dev_num=`echo $dev \\* $QAT_DHC3XXX_NUM_VFS + $vf_dev | bc`
                install -D -m 640 $ICP_BUILD_OUTPUT/c3xxxvf_dev0.conf.vm \
                        /etc/c3xxxvf_dev${vf_dev_num}.conf
            done
        done
    else
        # SRIOV on the guest. Install config file for each VF detected.
        for (( dev=0; dev<$numDh895xDevicesV; dev++ ))
        do
            install -D -m 640 $ICP_BUILD_OUTPUT/dh895xccvf_dev0.conf.vm /etc/dh895xccvf_dev${dev}.conf
        done
        for (( dev=0; dev<$numC62xDevicesV; dev++ ))
        do
            install -D -m 640 $ICP_BUILD_OUTPUT/c6xxvf_dev0.conf.vm /etc/c6xxvf_dev${dev}.conf
        done
        for (( dev=0; dev<$numD15xxDevicesV; dev++ ))
        do
            install -D -m 640 $ICP_BUILD_OUTPUT/d15xxvf_dev0.conf.vm /etc/d15xxvf_dev${dev}.conf
        done
        for (( dev=0; dev<$numC3xxxDevicesV; dev++ ))
        do
            install -D -m 640 $ICP_BUILD_OUTPUT/c3xxxvf_dev0.conf.vm /etc/c3xxxvf_dev${dev}.conf
        done
    fi

    echo "Creating startup and kill scripts"
    install -D -m 750 $ICP_BUILD_OUTPUT/qat_service /etc/init.d
    install -D -m 750 $ICP_BUILD_OUTPUT/adf_ctl /usr/sbin
    if [ $1 == "1" -o $1 == "2" ]; then
        # SRIOV on the host. Enable SRIOV in the qat_service 
        echo "# Comment or remove next line to disable sriov" > /etc/default/qat
        echo "SRIOV_ENABLE=1" >> /etc/default/qat
    else
        # Disable sriov for qat_service
        echo "# Remove comment on next line to enable sriov" > /etc/default/qat
        echo "#SRIOV_ENABLE=1" >> /etc/default/qat
    fi

    if [ $OS != "x86_64" ]; then
        echo "Copying libqat_s.so,libusdm_drv_s.so to /lib"
        cp $ICP_BUILD_OUTPUT/libqat_s.so $ICP_BUILD_OUTPUT/libusdm_drv_s.so /lib
    else
        if [ -d /lib64 ]; then
            echo "Copying libqat_s.so,libusdm_drv_s.so to /lib64"
            cp $ICP_BUILD_OUTPUT/libqat_s.so $ICP_BUILD_OUTPUT/libusdm_drv_s.so /lib64
        else
            echo "Copying libqat_s.so,libusdm_drv_s.so to /lib"
            cp $ICP_BUILD_OUTPUT/libqat_s.so $ICP_BUILD_OUTPUT/libusdm_drv_s.so /lib
        fi
    fi
    install -D -m 750 $ICP_BUILD_OUTPUT/usdm_drv.ko  "/lib/modules/`\uname -r`/kernel/drivers" 
    echo "Creating module.dep file for QAT released kernel object"
    echo "This will take a few moments"
    /sbin/depmod -a

    if [ -e /sbin/chkconfig ] ; then
        chkconfig --add qat_service
    elif [ -e /usr/sbin/update-rc.d ]; then
        update-rc.d qat_service defaults
    fi

    echo "Starting QAT service"
    /etc/init.d/qat_service shutdown
    sleep 3
    /etc/init.d/qat_service start

         
    if [ `lsmod |egrep -c "usdm_drv"` == "0" ]; then
        echo -e "\n\t*** Error:usdm_drv module not installed ***\n"
    fi
    if [ $numDh895xDevicesP != 0 ];then
        if [ `lsmod |egrep -c "qat_dh895xcc"` == "0" ]; then
            echo -e "\n\t*** Error:qat_dh895xcc module not installed ***\n"
        fi
        if [ $1 == "1" ]; then
            if [ `lsmod |egrep -c "qat_dh895xccvf"` == "0" ]; then
                echo -e "\n\t*** Error:qat_dh895xccvf module not installed ***\n"
            fi
        fi
    fi
    if [ $numC62xDevicesP != 0 ];then
        if [ `lsmod |egrep -c "qat_c62x"` == "0" ]; then
            echo -e "\n\t*** Error:qat_c62x module not installed ***\n"
        fi
        if [ $1 == "1" ]; then
            if [ `lsmod |egrep -c "qat_c62xvf"` == "0" ]; then
                echo -e "\n\t*** Error:qat_c62xvf module not installed ***\n"
            fi
        fi
    fi
    if [ $numD15xxDevicesP != 0 ];then
        if [ `lsmod |egrep -c "qat_d15xx"` == "0" ]; then
            echo -e "\n\t*** Error:qat_d15xx module not installed ***\n"
        fi
        if [ $1 == "1" ]; then
            if [ `lsmod |egrep -c "qat_d15xxvf"` == "0" ]; then
                echo -e "\n\t*** Error:qat_d15xxvf module not installed ***\n"
            fi
        fi
    fi
    if [ $numC3xxxDevicesP != 0 ];then
        if [ `lsmod |egrep -c "qat_c3xxx"` == "0" ]; then
            echo -e "\n\t*** Error:qat_c3xxx module not installed ***\n"
        fi
        if [ $1 == "1" ]; then
            if [ `lsmod |egrep -c "qat_c3xxxvf"` == "0" ]; then
                echo -e "\n\t*** Error:qat_c3xxxvf module not installed ***\n"
            fi
        fi
    fi
    if [ $1 == "2" ]; then
        if [ $numDh895xDevicesV != 0 ];then
            if [ `lsmod |egrep -c "qat_dh895xccvf"` == "0" ]; then
                echo -e "\n\t*** Error:qat_dh895xccvf module not installed ***\n"
            fi
        fi
        if [ $numC62xDevicesV != 0 ];then
            if [ `lsmod |egrep -c "qat_c62xvf"` == "0" ]; then
                echo -e "\n\t*** Error:qat_c62xvf module not installed ***\n"
            fi
        fi
        if [ $numD15xxDevicesV != 0 ];then
            if [ `lsmod |egrep -c "qat_d15xxvf"` == "0" ]; then
                echo -e "\n\t*** Error:qat_d15xxvf module not installed ***\n"
            fi
        fi
        if [ $numC3xxxDevicesV != 0 ];then
            if [ `lsmod |egrep -c "qat_c3xxxvf"` == "0" ]; then
                echo -e "\n\t*** Error:qat_c3xxxvf module not installed ***\n"
            fi
        fi
    fi
    if [ `$ICP_BUILD_OUTPUT/adf_ctl status | grep -c "state: down"` != "0" ]; then
        echo -e "\n\t*** Error:QAT driver not activated ***\n"
    fi

    if [ `grep -i "Error:" $ICP_ROOT/InstallerLog.txt | wc -l` != "0" ]; then
        echo -e "\n\t******** An error was detected in InstallerLog.txt file********\n"
        ERR_COUNT=1
        return 1
    else
        echo -e "\n\t*** No error detected in InstallerLog.txt file ***\n"
        ERR_COUNT=0
        return 0
    fi

}

UninstallAccel()
{
    if [ `lsmod | grep "qat" | wc -l` != "0" -o -e $KERNEL_SOURCE_ROOT_UPDATE/drivers/crypto/qat/qat_common/intel_qat.ko ]; then
        echo "Removing the QAT firmware"
        for bin_obj in $BIN_LIST
        do
            if [ -e /lib/firmware/$bin_obj ]; then
                /bin/rm -f /lib/firmware/$bin_obj
            fi
            if [ -e /lib/firmware/qat_fw/$bin_obj ]; then
                /bin/mv /lib/firmware/qat_fw/$bin_obj /lib/firmware/$bin_obj
            fi
        done

        if [ -d /lib/firmware/qat_fw ]; then
            /bin/rm -rf /lib/firmware/qat_fw
        fi

        if [ `lsmod | grep "usdm_drv" | wc -l` != "0" ]; then
            sudo rmmod usdm_drv
        fi

        # Check if one of the /etc/init.d script exist
        if [ -e /etc/init.d/qat_service ]; then
            /etc/init.d/qat_service shutdown
            if [ -e /sbin/chkconfig ] ; then
                chkconfig --del qat_service
            elif [ -e /usr/sbin/update-rc.d ]; then
                update-rc.d -f qat_service remove
            fi
            /bin/rm -f /etc/init.d/qat_service
            /bin/rm -f /usr/sbin/adf_ctl
        fi

        if [ $OS != "x86_64" ]; then
            rm -f /lib/libqat_s.so /lib/libusdm_drv_s.so
        else
            if [ -d /lib64 ]; then
                rm -f /lib64/libqat_s.so /lib64/libusdm_drv_s.so
            else
                rm -f /lib/libqat_s.so /lib/libusdm_drv_s.so
            fi
        fi

        echo "Removing config files"
        /bin/rm -f /etc/dh895xcc*.conf
        /bin/rm -f /etc/c6xx*.conf
        /bin/rm -f /etc/c3xxx*.conf

        echo "Removing calgary/canterbury files"
        /bin/rm -rf /lib/firmware/calgary /lib/firmware/calgary32 /lib/firmware/canterbury

        echo "Removing drivers modules"
        /bin/rm -rf $KERNEL_SOURCE_ROOT_UPDATE/drivers/crypto/qat/
        /bin/rm -f "/lib/modules/`\uname -r`/kernel/drivers"/usdm_drv.ko
        if [ `lsmod |egrep -c "usdm_drv|intel_qat"` != "0" ]; then
            echo -e "\n\t*** Error:Some modules not Removed Properly***\n"
            echo -e "\n\n\n\n\t***************************************"
            echo -e "\n\t*** Acceleration Uninstall Failed ***>"
            echo -e "\n\t***************************************\n\n\n\n\n"
            return 1
        else
            echo -e "\n\n\n\n\t***************************************"
            echo -e "\n\t*** Acceleration Uninstall Complete ***>"
            echo -e "\n\t***************************************\n\n\n\n\n"
            return 0
        fi
    else
        echo -e "\n\n\t*** FAIL Acceleration Package Not Installed ***>\n\n\n"
        return 1
    fi

}

PkgDependList()
{
    cd $ICP_ROOT/quickassist
    make depend_linux
    return 0;
}

RemoveLogFile()
{
    echo -e "\n\n\n\t**********************************************"
    echo -e "\t* Removing log file in Build Location : $ICP_ROOT"
    rm -f $ICP_ROOT/InstallerLog.txt
    echo -e "\t************************************************\n\n\n\n\n"

}

PrintInstallOtions()
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

ProbeAccDeviceInfo()
{
    if [ -z $ICP_ROOT ]; then
        #echo "string is empty, apply Default"
        ICP_ROOT="$INSTALL_DEFAULT"
    fi
#    echo -e "Number of C62X devices on the system:$numC62xDevice\n"
    numC62xDevice=`lspci -vnd 8086: | egrep -c "37c8|37c9"`
#    echo -e "Number of D15XX devices on the system:$numD15xxDevice\n"
    numD15xxDevice=`lspci -vnd 8086: | egrep -c "6f54|6f55"`
    numDh895xDevice=`lspci -vnd 8086: | egrep -c "0435|0443"`
#    echo -e "Number of DH895xCC devices on the system:$numDh895xDevice\n"
    numC3xxxDevice=`lspci -vnd 8086: | egrep -c "19e2|19e3"`

    if (($numDh895xDevice != "0"));then
        ICP_DEV_TYPE_QAT16="QAT1.6"
    fi
    if (($numC62xDevice != "0"));then
        ICP_DEV_TYPE_QAT17="QAT1.7"
    fi
    if (($numD15xxDevice != "0"));then
        ICP_DEV_TYPE_QAT17="QAT1.71"
    fi
    if (($numC3xxxDevice != "0"));then
        ICP_DEV_TYPE_nQAT17="nQAT1.7"
    fi
}

# set the number of each type of device and the device ID number
SetDevices()
{
    # store the total number of each type of device
    numDh895xDevicesP=$(lspci -n | egrep -c "$INTEL_VENDORID:$DH895_DEVICE_NUMBER")
    numDh895xDevicesV=$(lspci -n | egrep -c "$INTEL_VENDORID:$DH895_DEVICE_NUMBER_VM")
    numC62xDevicesP=$(lspci -n | egrep -c "$INTEL_VENDORID:$C62X_DEVICE_NUMBER")
    numC62xDevicesV=$(lspci -n | egrep -c "$INTEL_VENDORID:$C62X_DEVICE_NUMBER_VM")
    numC3xxxDevicesP=$(lspci -n | egrep -c "$INTEL_VENDORID:$C3XXX_DEVICE_NUMBER")
    numC3xxxDevicesV=$(lspci -n | egrep -c "$INTEL_VENDORID:$C3XXX_DEVICE_NUMBER_VM")
    numD15xxDevicesP=$(lspci -n | egrep -c "$INTEL_VENDORID:$D15XX_DEVICE_NUMBER")
    numD15xxDevicesV=$(lspci -n | egrep -c "$INTEL_VENDORID:$D15XX_DEVICE_NUMBER_VM")
    ProbeAccDeviceInfo
}

# display stepping info lines for one dev type
# parameter 1 is the device number
# parameter 2 is the dev count
DisplayDevStepping()
{
    lin=1
    while [ $lin -le $2 ]
    do
        DetectStepping $1 $lin
        if [ "$?" -ne "0" ]; then
            break
        fi
        ((lin++))
    done
}

# list the stepping info for each present device
DisplaySteppings()
{
    # display a detection line for each device
    DisplayDevStepping $DH895_DEVICE_NUMBER $numDh895xDevicesP
    DisplayDevStepping $DH895_DEVICE_NUMBER_VM $numDh895xDevicesV
    DisplayDevStepping $C62X_DEVICE_NUMBER $numC62xDevicesP
    DisplayDevStepping $C62X_DEVICE_NUMBER_VM $numC62xDevicesV
    DisplayDevStepping $D15XX_DEVICE_NUMBER $numD15xxDevicesP
    DisplayDevStepping $D15XX_DEVICE_NUMBER_VM $numD15xxDevicesV
    DisplayDevStepping $C3XXX_DEVICE_NUMBER $numC3xxxDevicesP
    DisplayDevStepping $C3XXX_DEVICE_NUMBER_VM $numC3xxxDevicesV
    echo
}

MainViewGeneral()
{

    #Print Disclaimer
    #================
    WelcomeMessage
    #set permissions to create owner and group files only
    umask 7
    loop=0
    SetDevices
    DisplaySteppings


    while [ $loop = 0 ];
    do
        if [ -z $CL_INSTALL_OPTION ]; then
           #Switch Statement to Select Installing Option
           #============================================
           echo -e "\t Please Select Install Option :"
           echo -e "\t ----------------------------"
           echo -e "\t 1  Build Acceleration"
           echo -e "\t 2  Install Acceleration"
           echo -e "\t 3  Install SR-IOV Host Acceleration"
           echo -e "\t 4  Install SR-IOV Guest Acceleration"
           echo -e "\t 5  Show Acceleration Device Information"
           echo -e "\t 6  Build Acceleration Sample Code"
           echo -e "\t 7  Uninstall"
           echo -e "\t 8  Dependency List"
           echo -e "\t 9  Build Acceleration and Sample Code (DC_ONLY)"
           echo -e "\t 0  Exit"
           echo -e "\n"
           read case

        fi

        # set the device counts every loop to ensure it is updated, easier than 
        SetDevices

        if [ ! -z $case ]; then
            if [ $case != "1" -a $case != "ba" -a $case != "5" -a $case != "bs" -a $case != "6" -a $case != "9" ]; then
                # If there are no devices present exit
                if [ $(($numDh895xDevicesP + $numDh895xDevicesV + $numC62xDevicesP + $numC62xDevicesV + $numD15xxDevicesP +  $numD15xxDevicesV + $numC3xxxDevicesP + $numC3xxxDevicesV)) -eq 0 ]; then
                    echo -e "\n\t****Devices not Available****>\n\n"
                    exit 1
                fi
            fi
        fi

        case $case in

            1 | ba)
                echo -e "\n\t****Building Acceleration****>\n\n"
                CreateBuildFolders
                SetENV
                RemoveLogFile
                PrintInstallOtions
                sleep 3
                BuildAccel 2>&1 | tee -a InstallerLog.txt;(exit ${PIPESTATUS[0]})
                InstallerOperationStatus=$?
                if [ "$InstallerOperationStatus" != "0" ];then
                    echo -e "\n\t***Error observed in Acceleration Build***>\n\n" | tee -a InstallerLog.txt
                else 
                    echo -e "\n\n\n\t***Acceleration Build Complete***>\n\n"
                fi

            ;;

            2 | a)
                echo -e "\n\t****Installing Acceleration****>\n\n"
                CreateBuildFolders
                SetENV
                RemoveLogFile
                PrintInstallOtions
                sleep 3
                InstallAccel 0 2>&1 | egrep -v 'SSL error' | tee -a InstallerLog.txt;(exit ${PIPESTATUS[0]})
                InstallerOperationStatus=$?
                if [ "$InstallerOperationStatus" != "0" ];then
                    echo -e "\n\t***Error observed in Acceleration Installation***>\n\n" | tee -a InstallerLog.txt
                else
                    echo -e "\n\n\n\t***Acceleration Installation Complete***>\n\n"
                fi

            ;;

            3 | ha)
                echo -e "\n\t****Installing SRIOV Host Acceleration****>\n\n"
                CreateBuildFolders
                SetENV
                RemoveLogFile
                PrintInstallOtions
                sleep 3
                InstallAccel 1 2>&1 | egrep -v 'SSL error' | tee -a InstallerLog.txt;(exit ${PIPESTATUS[0]})
                InstallerOperationStatus=$?
                if [ "$InstallerOperationStatus" != "0" ];then
                    echo -e "\n\t***Error observed in Acceleration Installation(SRIOV-Host)***>\n\n" | tee -a InstallerLog.txt
                else
                    echo -e "\n\n\n\t***Acceleration Installation(SRIOV-Host) Complete***>\n\n"
                fi

            ;;

            4 | ga)
                echo -e "\n\t****Installing SRIOV Guest Acceleration****>\n\n"
                CreateBuildFolders
                SetENV
                RemoveLogFile
                PrintInstallOtions
                sleep 3
                InstallAccel 2 2>&1 | egrep -v 'SSL error' | tee -a InstallerLog.txt;(exit ${PIPESTATUS[0]})
                InstallerOperationStatus=$?
                if [ "$InstallerOperationStatus" != "0" ];then
                    echo -e "\n\t***Error observed in Acceleration Installation(SRIOV-Guest)***>\n\n" | tee -a InstallerLog.txt
                else
                    echo -e "\n\n\n\t***Acceleration Installation(SRIOV-Guest) Complete***>\n\n"
                fi

            ;;

            5 )
                echo -e "\n\t****Acceleration Information****>\n\n"
                PrintAccDeviceInfo

            ;;

            6 | bs)
                echo -e "\n\t****Building Acceleration Sample Code****>\n\n"
                CreateBuildFolders
                SetENV
                RemoveLogFile
                PrintAccSampleInstallOptions
                sleep 3
                BuildAccelSample 2>&1 | tee -a InstallerLog.txt;(exit ${PIPESTATUS[0]})
                InstallerOperationStatus=$?
                if [ "$InstallerOperationStatus" != "0" ];then
                    echo -e "\n\t***Error observed in Acceleration Sample Code Build***>\n\n" | tee -a InstallerLog.txt
                else
                    echo -e "\n\n\n\t***Acceleration Sample Code Build Complete***>\n\n"
                fi

            ;;

            7 | u)
                echo -e "\n\t****Uninstall****>\n\n"
                UninstallAccel
                InstallerOperationStatus=$?
                SetDevices

            ;;

            8)
                echo -e "\n\t****PACKAGE DEPENDENCY List****>\n\n"
                CreateBuildFolders
                SetENV
                PkgDependList
            ;;

            9)
                echo -e "\n\t****Build Acceleration and Sample Code (DC_ONLY)****>\n\n"
                CreateBuildFolders
                SetENV
                export ICP_DC_ONLY=1
                export DO_CRYPTO=0
                RemoveLogFile
                PrintInstallOtions
                sleep 3
                BuildAccel 2>&1 | tee -a InstallerLog.txt;(exit ${PIPESTATUS[0]})
                InstallerOperationStatus=$?
                if [ "$InstallerOperationStatus" != "0" ];then
                    echo -e "\n\t***Error observed in Acceleration Build(DC_ONLY)***>\n\n" | tee -a InstallerLog.txt
                else
                    echo -e "\n\n\n\t***Acceleration Build Complete(DC_ONLY)***>\n\n"
                fi
                unset ICP_DC_ONLY
                sleep 3
                BuildAccelSample 2>&1 | tee -a InstallerLog.txt;(exit ${PIPESTATUS[0]})
                InstallerOperationStatus=$?
                if [ "$InstallerOperationStatus" != "0" ];then
                    echo -e "\n\t***Error observed in Acceleration Sample Code Build(DC_ONLY)***>\n\n" | tee -a InstallerLog.txt
                else
                    echo -e "\n\n\n\t***Acceleration Sample Code Build Complete(DC_ONLY)***>\n\n"
                fi
                unset DO_CRYPTO
            ;;

            0)
                echo -e "\n\t****EXITING INSTALLER****>\n\n"
                exit
            ;;

            h | -h)
                #echo -e "\n\t****Installer Command Line Help****>\n\n"
                echo -e "\t\t================="
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
            if [ "$InstallerOperationStatus" != "0" ];then
               echo -e "\n\n\n\t***installer operation status is Failure***>\n\n" | tee -a InstallerLog.txt
               exit 1
            else
               echo -e "\n\n\n\t***installer operation status is Success***>\n\n" | tee -a InstallerLog.txt
               exit 0
            fi
        fi
        #set umask back to original user setting
        umask $ORIG_UMASK
    done

}

MainViewGeneral
