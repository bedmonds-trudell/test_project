
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sapp)

zephyr_include_directories(../../lib/sapp)

target_sources(app PRIVATE src/main.c
               src/sapp_example.c
               ../../lib/sapp/sapp.c)