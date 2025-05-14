cmake_minimum_required(VERSION 3.13)

set(ENV{PICO_BOARD} "adafruit_feather_rp2040_usb_host")

## We want our modified lwIP (mainy SVG-support in httpd and zopfli compression)
set(ENV{PICO_LWIP_PATH} ${CMAKE_CURRENT_LIST_DIR}/lib/lwip)

## pico_sdk_import.cmake is copied from
## https://github.com/raspberrypi/pico-sdk/blob/master/external/pico_sdk_import.cmake
include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)

## Give the baby a name. Will also be the name of the resulting binaries
project(rp2040-sx1280-on-Adafruit-USB-host)

## Init the pico-sdk. Will instruct the user what to do if deps are missing
pico_sdk_init()

## Get the targets for "npm install" and "npm run build"
include(${CMAKE_CURRENT_LIST_DIR}/wwwsrc/CMakeLists.txt)

## Command and target to build makefsdata
if(DEFINED ENV{CI})
    message(STATUS "CI environment detected, using ZOPFLI compression")
    set(BUILD_MAKEFSDATA_PARAMS -DMAKEFS_SUPPORT_DEFLATE_ZOPFLI=1 -I/usr/include/zopfli)
    set(BUILD_MAKEFSDATA_LDPARAMS -lzopfli)
else()
    message(STATUS "No CI environment detected, using faster zlib compression")
    set(BUILD_MAKEFSDATA_PARAMS -DMAKEFS_SUPPORT_DEFLATE_ZLIB=1)
    set(BUILD_MAKEFSDATA_LDPARAMS "")
endif()
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/makefsdata
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/lib/lwip/src/apps/http/makefsdata/makefsdata.c
    COMMAND gcc -o makefsdata -DMAKEFS_SUPPORT_DEFLATE=1 ${BUILD_MAKEFSDATA_PARAMS} -I${CMAKE_CURRENT_LIST_DIR}/lib/lwip/src/include -I${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/lib/lwip/src/apps/http/makefsdata/makefsdata.c -lz ${BUILD_MAKEFSDATA_LDPARAMS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_target(makefsdata
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/makefsdata
)

## Command and target to build fsdata.c from the webapp using makefsdata
if(DEFINED ENV{CI})
    message(STATUS "CI environment detected, using ZOPFLI compression")
    set(MAKEFSDATA_PARAMS -defl:400 -11)
else()
    message(STATUS "No CI environment detected, using faster zlib compression")
    set(MAKEFSDATA_PARAMS -defl:6 -11)
endif()
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_LIST_DIR}/src/fsdata.c
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/wwwsrc/dist.md5
    COMMAND ./makefsdata ${CMAKE_CURRENT_LIST_DIR}/wwwsrc/dist ${MAKEFSDATA_PARAMS} -f:${CMAKE_CURRENT_LIST_DIR}/src/fsdata.c
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_target(fsdata
    DEPENDS webui_build makefsdata ${CMAKE_CURRENT_LIST_DIR}/src/fsdata.c
)

## Include the CMakeLists-files of (non-pico-sdk)-libraries we will be using
include(${CMAKE_CURRENT_LIST_DIR}/lib/libb64/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/lib/jsoncpp/interfaceLibForPicoSDK.cmake)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/lib/RadioLib" "${CMAKE_CURRENT_BINARY_DIR}/RadioLib")

## Extra stuff from TinyUSB, that is not part of tinyusb_device library
set(PICO_TINYUSB_PATH ${PICO_SDK_PATH}/lib/tinyusb)
set(TINYUSB_LIBNETWORKING_SOURCES
    ${PICO_TINYUSB_PATH}/lib/networking/dhserver.c
)

## We want C++20 and will fail if not supported
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## Get rid of some warnings that don't affect use since we build everything
## from source. See https://stackoverflow.com/a/48149400
add_compile_options(-Wno-psabi)

## Pass over the CI env var to the compiler
add_compile_definitions(CMAKE_VERSION=\"${CMAKE_VERSION}\")
if(DEFINED ENV{CI})
add_compile_definitions(CI_BUILD)
endif()

## Add our own C/C++ files here
## Sorted alphabetically
add_executable(${CMAKE_PROJECT_NAME}
    ${TINYUSB_LIBNETWORKING_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/src/log.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/stdio_usb.c
    ${CMAKE_CURRENT_LIST_DIR}/src/tusb_lwip_glue.c
    ${CMAKE_CURRENT_LIST_DIR}/src/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/src/webserver.cpp
)

## Add our rp2040-PIO programs here
#pico_generate_pio_header(${CMAKE_PROJECT_NAME}
#    ${CMAKE_CURRENT_LIST_DIR}/src/tx16.pio
#)

## Config for the jsoncpp library
target_compile_definitions(${CMAKE_PROJECT_NAME}
    PUBLIC JSON_USE_EXCEPTION=0
)

## LWIP works a little bit different than the other libraries since
## it ships its own FileList
set(LWIP_DIR ${CMAKE_CURRENT_LIST_DIR}/lib/lwip)
set (LWIP_INCLUDE_DIRS
    "${LWIP_DIR}/src/include"
    "${CMAKE_CURRENT_LIST_DIR}/src/"
)
include(${LWIP_DIR}/src/Filelists.cmake)

## Needed so tusb_config.h is found by the tinyUSB-library included in the pico-sdk
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${LWIP_INCLUDE_DIRS}
    ${PICO_TINYUSB_PATH}/lib/networking
    ${CMAKE_CURRENT_LIST_DIR}/src/
)

## Print memory usage percentages after linking
string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage")

## Pull in all pico-sdk + non-pico-sdk libraries
## Sorted alphabetically
target_link_libraries(${CMAKE_PROJECT_NAME}
    hardware_pwm
    hardware_spi
    jsoncpp
    libb64
    lwipallapps
    lwipcore
    pico_multicore
    pico_stdlib
    pico_time
    pico_unique_id
    RadioLib
    tinyusb_board
    tinyusb_device
)

## Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${CMAKE_PROJECT_NAME})

## Make sure fsdata is ready when lwip wants it
add_dependencies(lwipcore fsdata)
add_dependencies(lwipallapps fsdata)
