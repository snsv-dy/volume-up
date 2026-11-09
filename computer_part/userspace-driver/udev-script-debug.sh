#!/bin/sh
a=12
# echo $(date) > /home/user1/ran-driver.txt

(chmod o+rw /dev/skel0)
chresult=$?
cat <<EOF > /root/rule-result$(date +"%H:%M:%S").txt
chresult: $chresult
ACTION: $ACTION
DEVPATH:    $DEVPATH
DEVTYPE:    $DEVTYPE
INTERFACE:  $INTERFACE
MODALIAS:   $MODALIAS
PRODUCT:    $PRODUCT
SEQNUM: $SEQNUM
SUBSYSTEM:  $SUBSYSTEM
TYPE:   $TYPE
USEC_INITIALIZED:   $USEC_INITIALIZED
device: $(ls -l /dev/skel0)
EOF

# source /home/user1/udev-script.sh
su user1 -c '(/home/user1/audio | logger)&'