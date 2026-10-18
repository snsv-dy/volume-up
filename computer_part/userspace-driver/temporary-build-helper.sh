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

# project("My Awesome Tested Project" C)  
# cmake_minimum_required(VERSION 3.0)  
# # Let's tell it a bit about our environment 
# SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)  
# # We declare a variable to select what to build 
# set (TARGET_GROUP production CACHE STRING "What Should I Build?")  
# # Modules that exist in both our release and our unit tests, are added now   
# add_subdirectory(src)   

# if(TARGET_GROUP STREQUAL release) 
#     # If we had any files that ONLY were compiled into our release, for example startup code,   
#     # it's really handy to put it here.  
# elseif(TARGET_GROUP STREQUAL test) 
#     # Our tests need to let CTest know we're running tests    
#     include(CTest)  
#     # It also needs to understand we have code in source and test directories  
#     add_subdirectory(unity)    
#     add_subdirectory(test)  
# else()    
#     message(FATAL_ERROR "I don't know the TARGET_GROUP you gave me!") 
# endif()