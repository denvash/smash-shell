cmake_minimum_required(VERSION 3.15)
project(OS_HW1_WET)

set(CMAKE_CXX_STANDARD 11)
set(GCC "-std=c++11 -Wall")

add_definitions(${GCC})

add_executable(smash
        smash/smash.cpp
        smash/commands.cpp
        smash/signals.cpp
        )