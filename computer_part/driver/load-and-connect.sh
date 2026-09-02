#!/bin/bash

PORT=5556
module_name=scull.ko
driver_path=driver

if [[ "$1" == "l" ]]; then
    # source ./copy-to-vm.sh
    if ! ssh -p ${PORT} root@localhost 'cd driver'; then
        if ! ssh -p ${PORT} root@localhost 'mkdir driver'; then
            echo "Failed to create directory"
            exit 1
        fi
    fi
    
    if ! scp -P ${PORT} ${module_name} scull_load scull_unload root@localhost:/root/driver/; then
        echo "Failed to copy files"
    fi
    
    if ! ssh -p ${PORT} root@localhost 'cd driver; ./scull_load'; then
        echo "Failed to load"
        # if ! ssh -p ${PORT} root@localhost '~/onstart.sh; cd /mnt/shared/; ./scull_load'; then
        # fi
    fi
    # echo "loadding and shell"
elif [[ "$1" == "lu" ]]; then
    echo "unload"
    if ! ssh -p ${PORT} root@localhost 'driver/scull_unload'; then
        echo "Failed to unload"
    fi
elif [[ "$1" == "r" ]]; then
    echo "reboot"

    ssh -p ${PORT} root@localhost reboot || exit 1
    sleep 1

    counter=1
    while ! ssh -p ${PORT} root@localhost uname -a; do
        echo "connecting attempt $counter"
        counter=$(($counter + 1))
        sleep 1
    done

    # Mount shared dir
    # ssh -p ${PORT} root@localhost '~/onstart.sh'
    echo "Reboot complete"
# elif [[ "$1" == "log" ]]; then
#     echo "printing logs"
#     ssh -p ${PORT} root@localhost 'killall klogd; bash -c \"cat /proc/kmsg \"' || exit 1
#     echo "restoring klog"
#     ssh -p ${PORT} root@localhost 'klogd -c 8' || exit 1
elif [[ "$1" == "logs" ]]; then
    if ! scp -P ${PORT} root@localhost:/var/log/kernel.log.* ./machine_logs/; then
        echo "Failed to fetch logs"
    fi
elif [[ "$1" == "make" ]]; then
    source /home/jacek/programy/linux_kernel/buildroot-2026.05.2/output/host/environment-setup
    KERNELDIR=/home/jacek/programy/linux_kernel/buildroot-2026.05.2/output/build/linux-6.18.7
    make
elif [[ "$1" == "gadget" ]]; then
    if ! scp -P ${PORT} configfs-gadget.sh root@localhost:/root; then
        echo "Failed to upload gadget script"
        exit 1
    fi
    if ! ssh -p ${PORT} root@localhost './configfs-gadget.sh'; then
        echo "Failed to execute gadget script"
    else
        echo "Gadget probably created"
        if ! ssh -p ${PORT} root@localhost 'echo 0xCAFE 0x4013 > /sys/bus/usb-serial/drivers/generic/new_id'; then
            echo "Failed to connect gadget to serial driver"
        else
            echo "serial driver connected to gadget"
        fi
    fi
elif [[ "$1" == "serial" ]]; then
    if ! ssh -p ${PORT} root@localhost 'picocom -b 9600 -i /dev/ttyGS0'; then
    # if ! ssh -p ${PORT} root@localhost 'cat /dev/ttyUSB0'; then
        echo "Failed to serial"
        exit 1
    fi
else
    echo "No action to take."
fi


# ssh -p ${PORT} root@localhost uname -a

# echo "after ssh"