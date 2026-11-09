#!/bin/bash

PORT=5556
module_name=scull.ko
driver_path=driver

USERUSED=root
USERHOME=/root
# USERUSED=user1
# USERHOME=/home/user1

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
    # KERNELDIR=/home/jacek/programy/linux_kernel/buildroot-2026.05.2/output/build/linux-6.18.7
    # Make drivers
    make
    # For FunctionFs handler
    $CC \
    -I${KERNELDIR}include/uapi/linux/usb/ \
    -D__force= -D__user= \
    -o build/gadgetfs_handler \
    gadgetfs_handler.c \
    -g -lpthread

    # For userspace part of the driver
    # (Move to makefile and the above as well idk.)
    # pushd ../userspace-driver/
    # source temporary-build-helper.sh
    # popd

    # audio.c and userspace driver.
    pushd ..
    cmake -B ./build-br
    cmake --build build-br
    popd

elif [[ "$1" == "gadget" ]]; then
    if ! scp -P ${PORT} configfs-gadget.sh $USERUSED@localhost:$USERHOME; then
        echo "Failed to upload gadget script"
        exit 1
    fi
    # Root since mount is in the script
    if ! ssh -p ${PORT} root@localhost "$USERHOME/configfs-gadget.sh"; then
        echo "Failed to execute gadget script"
    else
        echo "Gadget probably created"
        # if ! ssh -p ${PORT} $USERUSED@localhost 'echo 0xCAFE 0x4013 > /sys/bus/usb-serial/drivers/generic/new_id'; then
        #     echo "Failed to connect gadget to serial driver"
        # else
        #     echo "serial driver connected to gadget"
        # fi
    fi
elif [[ "$1" == "gadget-user" ]]; then
    if ! scp -P ${PORT} build/usb_temp.ko $USERUSED@localhost:$USERHOME; then
        echo "Failed to upload usb driver"
    fi 
    if ! ssh -p ${PORT} $USERUSED@localhost "insmod $USERHOME/usb_temp.ko"; then
        echo "Failed to insmod usb driver"
    fi

    if ! ssh -p ${PORT} $USERUSED@localhost 'mkdir -p functionfs && mount -t functionfs usb0 functionfs'; then
        echo "Failed to mount functionfs"
    fi

    if ! ssh -p ${PORT} $USERUSED@localhost 'rm gadgetfs_handler'; then
        echo "Failed to remove previous gadgetfs_handler"
    fi 

    if ! ssh -p ${PORT} user1@localhost 'pulseaudio --daemonize=yes --system=no --disallow-module-loading=1'; then
        echo "Failed to run pulseaudio daemon as user1"
    else
        echo "pulseaudio daemon should be running."
    fi

    if ! ssh -p ${PORT} root@localhost "rm /root/rule-result*.txt"; then
        echo "Failed to remove tag.txt file"
    fi

    if ! scp -P ${PORT} build/gadgetfs_handler $USERUSED@localhost:$USERHOME; then
        echo "Failed to upload gadgetfs_handler"
    else    
        if ! ssh -t -t -p ${PORT} $USERUSED@localhost 'cd functionfs && ../gadgetfs_handler'; then
            echo "Failed to run gadgetfs_handler"
        fi
        
        # ssh -p ${PORT} $USERUSED@localhost "
        # echo "dummy_udc.0" > configfs/usb_gadget/g1/UDC
        # sleep 1
        # chmod 0666 /dev/skel0
        # "

        # if ! ; then
        #     echo "could not make device rw for user."
        # fi
    fi 
elif [[ "$1" == "syslog" ]]; then
    if ! ssh -p ${PORT} user1@localhost 'tail -f /var/log/messages'; then
        echo "failed to read logs"
    fi
elif [[ "$1" == "device-in" ]]; then
    (ssh -p ${PORT} $USERUSED@localhost "
        echo "dummy_udc.0" > configfs/usb_gadget/g1/UDC
        ")

    if ! [[ $? ]]; then
        echo "Failed to activate the device (plug in)"
    fi
elif [[ "$1" == "device-out" ]]; then
(ssh -p ${PORT} $USERUSED@localhost "
        echo "" > configfs/usb_gadget/g1/UDC
        ")

    if ! $?; then
        echo "Failed to deactivate the device (plug out)"
    fi
elif [[ "$1" == "driver-audio" ]]; then
    if ! scp -P ${PORT} ../build-br/audio $USERUSED@localhost:$USERHOME; then
        echo "Failed to upload usb driver"
    fi

    # # Install rule for skel0 access without root
    # if ! ssh -p ${PORT} root@localhost 'rm /etc/udev/rules.d/97-volumeknob-access.rules'; then
    #     echo "access rule not deleted or not found."
    # fi
    # if ! scp -P ${PORT} ../userspace-driver/97-volumeknob-access.rules root@localhost:/etc/udev/rules.d/; then
    #     echo "could not upload access rule"
    # fi
    # start pulse daemon
    if ! ssh -p ${PORT} user1@localhost 'pulseaudio --daemonize=yes --system=no --disallow-module-loading=1'; then
        echo "failed to start pulse daemon. Exiting"
        exit 1
    fi
    # create user if not exist?

    # if ! ssh -t -t -p ${PORT} root@localhost './audio'; then
    #     echo "Failed to run audio with driver"
    # fi
elif [[ "$1" == "driver-uspace-shell" ]]; then
    if ! scp -P ${PORT} ../userspace-driver/build/UserspaceDriverImpl root@localhost:/root; then
        echo "Failed to upload usb driver"
    fi 

    if ! ssh -t -t -p ${PORT} root@localhost './UserspaceDriverImpl /dev/skel0'; then
        echo "Failed to run uspace driver"
    fi
    
elif [[ "$1" == "install-udev" ]]; then
    if ! ssh -t -t -p ${PORT} root@localhost 'rm /etc/udev/rules.d/*.rules'; then
        echo "Failed to remove previous udev rule"
    fi
    if ! scp -P ${PORT} ../userspace-driver/*.rules root@localhost:/etc/udev/rules.d/; then
        echo "Failed to upload udev rule"
    fi
    if ! scp -P ${PORT} ../userspace-driver/udev-script.sh user1@localhost:/home/user1; then
        echo "Failed to upload udev script"
    fi
    if ! scp -P ${PORT} ../userspace-driver/udev-script-debug.sh root@localhost:/root; then
        echo "Failed to upload udev debug script"
    fi
    if ! scp -P ${PORT} ../build-br/audio user1@localhost:/home/user1; then
        echo "Failed to upload userspace driver"
    fi
elif [[ "$1" == "serial" ]]; then
    if ! ssh -p ${PORT} root@localhost 'picocom -b 9600 -i /dev/ttyGS0'; then
    # if ! ssh -p ${PORT} root@localhost 'cat /dev/ttyUSB0'; then
        echo "Failed to serial"
        exit 1
    fi
elif [[ "$1" == "pipe" ]]; then
    if ! ssh -p ${PORT} root@localhost 'cat /dev/scullpipe0'; then
    # if ! ssh -p ${PORT} root@localhost 'cat /dev/ttyUSB0'; then
        echo "Failed to /dev/scullpipe0"
        exit 1
    fi
else
    echo "No action to take."
fi


# ssh -p ${PORT} root@localhost uname -a

# echo "after ssh"