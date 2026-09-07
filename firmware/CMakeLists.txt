# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(snowgauge_fw LANGUAGES C)

target_sources(app PRIVATE
  src/main.c
  src/sensor_rail.c
  src/lidar.c
  src/battery.c
  src/tilt_lsm6dsl.c
  src/power.c
  src/usb_pm.c
  src/measure.c
  src/record.c
  src/timekeeping.c
  src/storage.c
  src/ble_adv.c
  src/smp_mgmt.c
  src/config.c
  src/cal_gatt.c
  src/shell_cmds.c
)
# Sensor backend (Kconfig choice SNOWGAUGE_SENSOR; overlay-tsd20.conf selects the TSD20)
target_sources_ifdef(CONFIG_SNOWGAUGE_SENSOR_TFMINI app PRIVATE src/tfmini.c)
target_sources_ifdef(CONFIG_SNOWGAUGE_SENSOR_TSD20 app PRIVATE src/tsd20.c)
