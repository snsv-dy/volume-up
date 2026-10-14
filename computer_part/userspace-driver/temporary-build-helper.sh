#!/bin/bash

# Chciałem tą część userspace testować w wirtualnej maszynie,
# ale ostatecznie to powinno być częścią projektu computer_part.
# Nie chce mi się na razie myśleć jak poprawnie poukładać ten projekt
# więc to będę budował osobno.
# (idk, czy obraz buildroota ma pulseaudio TODO?)

PORT=5556
module_name=scull.ko
driver_path=driver

# if [[ "$1" == "l" ]]; then
source /home/jacek/programy/linux_kernel/buildroot-2026.05.2/output/host/environment-setup
cmake -B ./build
cmake --build build
# 
# fi