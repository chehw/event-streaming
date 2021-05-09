#!/bin/bash

declare -A module_devfiles
# add dependencies
module_devfiles=(
  ["json-c"]="libjson-c-dev"
  ["rdkafka"]="librdkafka-dev"
  ["libjwt"]="libjwt-dev"
  ["libsoup-2.4"]="libsoup-2.4-dev"
)

PKG_MGR="sudo apt-get -y "
PKG_CONFIG=$(which pkg-config)
if [ -z "$PKG_CONFIG" ]; then
    ${PKG_MGR} install build-essential
fi

mkdir -p obj/utils bin
[ $? -ne 0 ] && exit 1

# install libdb5.3-dev
LIBDB_DEVFILE="libdb5.3-dev"
if [ ! -e /usr/include/db.h ] ; then
    echo "install '${LIBDB_DEVFILE}' ..."
    ${PKG_MGR} install ${LIBDB_DEVFILE}
    [ $? -ne 0 ] && exit 1
fi

for mod in ${!module_devfiles[@]}
do
    echo "check module: $mod"
    echo -ne "    --> version="
    ${PKG_CONFIG} --modversion $mod 2>/dev/null

    if [ $? -ne 0 ]; then
        devfile=${module_devfiles[$mod]}
        echo -e "\e[31m(not found)\e[39m"
        echo -e "\e[33m    --> install dependency: $mod: $devfile" "\e[39m"
        ${PKG_MGR} install "$devfile" >/dev/null 2>&1
        ret=$?
        if [ $ret -eq 0 ]; then
            echo -ne "    --> version="
            ${PKG_CONFIG} --modversion $mod 2>/dev/null
            ret=$?
        fi
        [ $ret -ne 0 ] && exit $ret
    fi
done
