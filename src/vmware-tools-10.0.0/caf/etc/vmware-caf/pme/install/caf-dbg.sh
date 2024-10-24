#!/bin/bash

function prtHeader() {
   local header=$1

   echo "*************************"
   echo "***"
   echo "*** $header"
   echo "***"
   echo "*************************"
}

function setCafRootDir() {
   if [ "$CAF_CONFIG_DIR" = "" ]; then
      if [ -f "/etc/vmware-caf/pme/config/cafenv.config" ]; then
         source "/etc/vmware-caf/pme/config/cafenv.config"
      else
         if [ -f "/etc/vmware-caf/client/config/cafenv.config" ]; then
            source "/etc/vmware-caf/client/config/cafenv.config"
         else
            echo "Failed to resolve cafenv.config"
            exit 1
         fi
      fi
   fi
}

function validateNotEmpty() {
   local value=$1
   local name=$2

   if [ "$value" = "" ]; then
      echo "Value cannot be empty - $name"
      exit 1
   fi
}

function enableCaf() {
   local username="$1"
   local password="$2"
   validateNotEmpty "$username" "username"
   validateNotEmpty "$password" "password"

   egrep -qw "amqp_username|amqp_password" "$CAF_CONFIG_DIR/CommAmqpListener-appconfig"; isFnd="$?"
   if [ "$isFnd" = "1" ]; then
      sed -i "s/\[amqp\]/[amqp]\namqp_username=${username}\namqp_password=${password}/g" "$CAF_CONFIG_DIR/CommAmqpListener-appconfig"
   fi
}

function prtHelp() {
   echo "*** $0 cmd <args>"
   echo "  Runs various CAF commands"
   echo "    cmd: The CAF command to run:"
   echo "      * checkTunnel                    Checks the AMQP Tunnel "
   echo "      * checkCerts                     Checks the certificates"
   echo "      * checkCertsVerbose              Checks the certificates"
   echo ""
   echo "      * validateXml                    Validates the XML files against the published schema"
   echo ""
   echo "      * clearCaches                    Clears the CAF caches"
   echo "      * genBashEntries                 Generates entries for ~/.bashrc"
   echo "      * enableCaf username password    Enables CAF"
   echo "      * checkFsPerms                   Checks the permissions, owner and group of the major CAF directories and files"
   echo "    args: The arguments to the command"
}

function validateXml() {
   schemaArea="$1"
   schemaPrefix="$2"

   validateNotEmpty "$schemaArea" "schemaArea"
   validateNotEmpty "$schemaPrefix" "schemaPrefix"

   schemaRoot="http://10.25.57.32/caf-downloads"

   for file in $(find "$CAF_OUTPUT_DIR" -name '*.xml' -print0 2>/dev/null | xargs -0 egrep -IH -lw "${schemaPrefix}.xsd"); do
      prtHeader "Validating $schemaArea/$schemaPrefix - $file"
      xmllint --schema "${schemaRoot}/schema/${schemaArea}/${schemaPrefix}.xsd" "$file"; rc=$?
      if [ "$rc" != "0" ]; then
         exit $rc
      fi
   done
}

function checkCerts() {
   certDir="$1"

   validateNotEmpty "$certDir" "certDir"

   if [ ! -d "$certDir" ]; then
      echo "*** Certificate directory does not exist - $certDir"
      exit 1
   fi

   pushd $certDir > /dev/null

   prtHeader "Checking certs - $certDir"

   openssl rsa -in client-key.pem -check -noout
   openssl verify -check_ss_sig -x509_strict -CAfile cacert.pem client-cert.pem

   clientCertMd5=$(openssl x509 -noout -modulus -in client-cert.pem | openssl md5 | cut -d' ' -f2)
   clientKeyMd5=$(openssl rsa -noout -modulus -in client-key.pem | openssl md5 | cut -d' ' -f2)
   if [ "$clientCertMd5" == "$clientKeyMd5" ]; then
      echo "Client Cert and Key md5's match"
   else
      echo "*** Client Cert and Key md5's do not match"
      exit 1
   fi

   popd > /dev/null
}

function checkCertsVerbose() {
   certDir="$1"

   validateNotEmpty "$certDir" "certDir"

   if [ ! -d "$certDir" ]; then
      echo "*** Certificate directory does not exist - $certDir"
      exit 1
   fi

   pushd $certDir > /dev/null

   prtHeader "Checking $certDir/cacert.pem"
   openssl x509 -in cacert.pem -text -noout

   prtHeader "Checking $certDir/client-cert.pem"
   openssl x509 -in client-cert.pem -text -noout

   prtHeader "Checking /etc/vmware-tools/GuestProxyData/server/cert.pem"
   openssl x509 -in /etc/vmware-tools/GuestProxyData/server/cert.pem -text -noout

   popd > /dev/null
}

function checkTunnel() {
   certDir="$1"

   validateNotEmpty "$certDir" "certDir"

   if [ ! -d "$certDir" ]; then
      echo "*** Certificate directory does not exist - $certDir"
      exit 1
   fi

   pushd $certDir > /dev/null

   prtHeader "Connecting to tunnel"
   openssl s_client -connect localhost:6672 -key client-key.pem -cert client-cert.pem -CAfile cacert.pem -verify 10

   popd > /dev/null
}

function checkFsPerms() {
   local dirOrFile="$1"
   local permExp="$2"
   local userExp="$3"
   local groupExp="$4"

   validateNotEmpty "$dirOrFile" "dirOrFile"
   validateNotEmpty "$permExp" "permExp"

   if [ "$userExp" = "" ]; then
      userExp="root"
   fi
   if [ "$groupExp" = "" ]; then
      groupExp="root"
   fi

   local statInfo=( $(stat -c "%a %U %G" $dirOrFile) )
   local permFnd=${statInfo[0]}
   local userFnd=${statInfo[1]}
   local groupFnd=${statInfo[2]}

   if [ "$permExp" != "$permFnd" ]; then
      echo "*** Perm check failed - expected: $permExp, found: $permFnd, dir/file: $dirOrFile"
      exit 1
   fi

   if [ "$userExp" != "$userFnd" ]; then
      echo "*** User check failed - expected: $userExp, found: $userFnd, dir/file: $dirOrFile"
      exit 1
   fi

   if [ "$groupExp" != "$groupFnd" ]; then
      echo "*** Group check failed - expected: $groupExp, found: $groupFnd, dir/file: $dirOrFile"
      exit 1
   fi
}

function genBashEntries() {
   echo "export PATH=\$PATH:/root/bin"
   echo ""
   echo "alias ll='ls -talFh'"
   echo "alias llTm='ls -talFh'"
   echo "alias llSz='ls -SalFh'"
   echo "alias vi='vim'"
   echo "alias df='df -h'"
   echo ""
   echo "alias cdCafCfg='pushd $CAF_CONFIG_DIR >/dev/null'"
   echo "alias cdCafLib='pushd $CAF_LIB_DIR >/dev/null'"
   echo "alias cdCafBin='pushd $CAF_BIN_DIR >/dev/null'"
   echo "alias cdCafInp='pushd $CAF_INPUT_DIR >/dev/null'"
   echo "alias cdCafOut='pushd $CAF_OUTPUT_DIR >/dev/null'"
   if [ "$CAF_PYTHON_DIR" != "" ]; then
      echo "alias cdCafPy='pushd $CAF_PYTHON_DIR >/dev/null'"
   fi
   if [ "$CAF_INVOKERS_DIR" != "" ]; then
      echo "alias cdCafInv='pushd $CAF_INVOKERS_DIR >/dev/null'"
   fi
   if [ "$CAF_PROVIDERS_DIR" != "" ]; then
      echo "alias cdCafPrv='pushd $CAF_PROVIDERS_DIR >/dev/null'"
   fi
   echo "alias cdCafLog='pushd $CAF_LOG_DIR >/dev/null'"
   echo ""
   echo "ff() { find . -type f 2>/dev/null | egrep \"\$@\" ;}"
   echo "fiFile() { find . -type f -print0 2>/dev/null | xargs -0 egrep -IH \"\$@\" ;}"
   echo "caf-processes() { $CAF_CONFIG_DIR/../scripts/caf-processes.sh \"\$@\" ;}"
   echo "caf-dbg() { $CAF_CONFIG_DIR/../install/caf-dbg.sh \"\$@\" ;}"
}

if [ $# -lt 1 -o "$1" = "--help" ]; then
   prtHelp
   exit 1
fi

cmd=$1
shift

setCafRootDir
certDir="$CAF_INPUT_DIR/data/input/certs"
scriptsDir="$CAF_CONFIG_DIR/../scripts"

case "$cmd" in
   "validateXml")
      validateXml "fx" "CafInstallRequest"
      validateXml "fx" "DiagRequest"
      validateXml "fx" "Message"
      validateXml "fx" "MgmtRequest"
      validateXml "fx" "MultiPmeMgmtRequest"
      validateXml "fx" "ProviderInfra"
      validateXml "fx" "ProviderRequest"
      validateXml "fx" "Response"
      validateXml "cmdl" "ProviderResults"
   ;;
   "checkCerts")
      checkCerts "$certDir"
   ;;
   "checkCertsVerbose")
      checkCertsVerbose "$certDir"
   ;;
   "checkTunnel")
      checkTunnel "$certDir"
   ;;
   "clearCaches")
      prtHeader "Clearing the CAF caches"
      rm -rf $CAF_OUTPUT_DIR/schemaCache/* $CAF_OUTPUT_DIR/comm-wrk/* $CAF_OUTPUT_DIR/providerHost/* $CAF_OUTPUT_DIR/responses/* $CAF_OUTPUT_DIR/events/* $CAF_LOG_DIR/*
   ;;
   "genBashEntries")
      genBashEntries
   ;;
   "enableCaf")
      enableCaf "$1" "$2"
   ;;
   "checkFsPerms")
      checkFsPerms "$CAF_INPUT_DIR" "755"
      checkFsPerms "$CAF_OUTPUT_DIR" "770"
      checkFsPerms "$CAF_CONFIG_DIR" "775"
      checkFsPerms "$CAF_LOG_DIR" "770"
      checkFsPerms "$CAF_BIN_DIR" "755"
      checkFsPerms "$CAF_LIB_DIR" "755"
   ;;
   *)
      echo "Bad command - $cmd"
      prtHelp
      exit 1
esac
