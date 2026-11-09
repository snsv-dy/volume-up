#!/bin/sh

echo $(date) > /home/user1/ran-driver.txt
su user1 -c '(/home/user1/audio | logger)&'