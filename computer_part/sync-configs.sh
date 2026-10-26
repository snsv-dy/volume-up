#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Ten plik powinien exportować zmienne BUILDROOT_ LINUX_, w których znajdują się configi owych projektów.
# (LINUX_ będzie wewnątrz buildroota)
source $SCRIPT_DIR/configs-paths.sh

echo "BUILDROOT_: $BUILDROOT_"
echo "    LINUX_: $LINUX_"

CONFIGS_PATH=buildroot-configs

mkdir $SCRIPT_DIR/$CONFIGS_PATH

cp $BUILDROOT_/.config $SCRIPT_DIR/$CONFIGS_PATH
cp $LINUX_/.config $SCRIPT_DIR/$CONFIGS_PATH/.config-linux
