#!/bin/sh

mkdir configfs
mount none configfs -t configfs
mkdir configfs/usb_gadget/g1
cd configfs/usb_gadget/g1
echo 0xcafe > idVendor
echo 0x4013 > idProduct 
mkdir strings/0x409
echo 0x03 > strings/0x409/serialnumber
echo TinyUSB > strings/0x409/manufacturer 
echo Knob > strings/0x409/product
mkdir strings/0x409/xu.0
echo "Idk honestly" > strings/0x409/xu.0/s 
mkdir configs/c.1
mkdir configs/c.1/strings/0x409
echo "Also idk" > configs/c.1/strings/0x409/configuration
echo 120 > configs/c.1/MaxPower

echo "full-speed" > max_speed

mkdir functions/ffs.usb0
ln -s functions/ffs.usb0 configs/c.1
# mkdir functions/gser.usb0
# ln -s functions/gser.usb0 configs/c.1
# echo "dummy_udc.0" > UDC
# mkdir functions/ncm.usb0
# ln -s functions/ncm.usb0 configs/c.1

# for functionFS
# mkdir ./functionfs
# mount -t functionfs usb0 ./functionfs

# 1. configure configFS
# 2. mount and configure functionFS
# 3. and then echo dummy_udc.0 > UDC