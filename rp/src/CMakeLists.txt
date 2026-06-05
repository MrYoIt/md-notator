cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK based on PICO_SDK_PATH
# NOTE: PICO_SDK_PATH must be set in the environment
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(md-notator C CXX ASM)

pico_sdk_init()

# Add the executable
add_executable(md-notator
    main.c
    notator_dongle.c
    emul.c
    aconfig.c
    blink.c
    display.c
    display_term.c
    download.c
    gconfig.c
    network.c
    reset.c
    romemul.c
    commemul.c
    chandler.c
    sdcard.c
    select.c
    term.c
    tprotocol.c
    util.c
)

# Include directories
target_include_directories(md-notator PRIVATE
    include
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Link libraries
target_link_libraries(md-notator
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_dma
    hardware_flash
    hardware_sync
    hardware_spi
    hardware_i2c
    hardware_rtc
    fatfs
    lwipopts
    pico_lwip
    pico_lwip_nosys
    pico_cyw43_arch_lwip_threadsafe_background
    tinyusb_device
    tinyusb_board
)

# Enable USB output, disable UART output
pico_enable_stdio_usb(md-notator 1)
pico_enable_stdio_uart(md-notator 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(md-notator)

# Compiler optimizations for speed (critical for bus timing)
target_compile_options(md-notator PRIVATE
    -O3
    -funroll-loops
    -fomit-frame-pointer
    -mcpu=cortex-m0plus
    -mthumb
)

target_link_options(md-notator PRIVATE
    -Wl,--gc-sections
)

# Print build info
message(STATUS "Building md-notator - SidecarTridge Notator Dongle Microfirmware")
message(STATUS "PICO_SDK_PATH: $ENV{PICO_SDK_PATH}")
