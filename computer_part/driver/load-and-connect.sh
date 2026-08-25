#!/bin/bash

if [[ "$1" == "l" ]]; then
    source ./copy-to-vm.sh
    
    if ! ssh -p 5555 root@localhost 'cd /mnt/shared/; ./scull_load'; then
        echo "Failed to load"
    fi
    # echo "loadding and shell"
elif [[ "$1" == "lu" ]]; then
    echo "unload"
    if ! ssh -p 5555 root@localhost '/mnt/shared/scull_unload'; then
        echo "Failed to unload"
    fi
elif [[ "$1" == "r" ]]; then
    echo "reboot"

    ssh -p 5555 root@localhost reboot || exit 1
    sleep 1

    counter=1
    while ! ssh -p 5555 root@localhost uname -a; do
        echo "connecting attempt $counter"
        counter=$(($counter + 1))
        sleep 1
    done

    # Mount shared dir
    ssh -p 5555 root@localhost '~/onstart.sh'
    echo "Reboot complete"
else
    echo "No action to take."
fi


# ssh -p 5555 root@localhost uname -a

# echo "after ssh"