#!/bin/bash

# BUILD_DIR=$(pwd)/build/module

# SCRIPT_PATH=$(dirname $(realpath $0))
# echo "SCRIPT_PATH $SCRIPT_PATH"

# export MOD_DIR=$SCRIPT_PATH/driver
pushd driver
make
cp usb_temp.ko ../build
make clean
popd