# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(snowgauge_fw LANGUAGES C)

target_sources(app PRIVATE
  src/main.c
  src/sensor_rail.c
  src/tfmini.c
  src/battery.c
  src/measure.c
  src/shell_cmds.c
)
