# Canarinv5-LEGO (Firmware for Canarin5 and Canarin6)
[![esp-idf build](https://github.com/intERLab-AIT/canarinv5-LEGO/actions/workflows/c-cpp.yml/badge.svg?branch=master)](https://github.com/intERLab-AIT/canarinv5-LEGO/actions/workflows/c-cpp.yml)


## Clone

```git clone --recurse-submodules git@github.com:intERLab-AIT/canarinv5-LEGO.git```

## Setup ESP-IDF environment

This project uses esp-idf v4.4.x. Please follow the instructions to set up the esp-idf environment for development.

## Choosing the right board

This firmware can compile for both Canarinv5 and Canarin6 boards.

First set the correct target:

```bash
idf.py set-target esp32 # for canarin 5
#or
idf.py set-target esp32s3 # for canarin 6
```

Then copy the correct board configuration file:

```bash
cp sdconfig.esp32 sdkconfig # for canarin 5
#or
cp sdconfig.esp32s3 sdkconfig # for canarin 6
```

Then you can build and flash the firmware as usual:

```bash
idf.py -p (PORT) build flash monitor
```


## Upload firmware to over the air (OTA) remote server
```bash
./scripts/upload-binary.sh <branch> <binary>
```
branch: can5 or can6

binary: typically build/can5-app.bin

Example:
```bash
USER=raunak PASSWORD=xxxx ./deploy.sh can5 build/can5-app.bin
```


The script uploads the firmware to esp-ota.hazemon.in.th.


## JTAG DEBUG
https://www.visualmicro.com/page/ESP32-Debugging.aspx
https://www.visualmicro.com/pics/Debug-Help-ESP32-Jlink-Connections.png


## Changing default configuration


The default configuration is defined in `components/can5_config/can5_config_types.c`. Wifi ssid, password, lora parameters, etc. can be edited here.
To apply the configuration 

`config_manager.factory_default()`

needs to be uncommented in `__can5_init_can5_config()` in `main/src/can5_init.c:103`.

## Clone

```git clone --recurse-submodules git@github.com:intERLab-AIT/canarinv5-LEGO.git```


## Cmake
Please use cmake version 3.22.1. Newer cmake does not honour c++17 standard set in esp_modem component.

## Linux host test

Some files can be compiled on (linux) host system to test business logic. This enables TDD with proper use of Valgrind, gcov and gdb.
To run it:


```
$ cd linux_host_test
$ mkdir build
$ cd build
$ cmake ..
$ make
$ ./test-app
```

Each component has `cmake_linuxtest/CMakeLists.txt` which should include the file for testing. Dependencies can be fulfilled to some degree by creating mock headers and code in `linux_host-test/mock` directory.
Do not load esp idf environment before using this test environment as idf environment changes the tool chain.


