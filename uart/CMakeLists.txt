cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(picoprobe)

pico_sdk_init()

add_executable(picoprobe
        src/main.c
        src/usb_descriptors.c
        src/cdc_uart.c
        src/get_serial.c
)

target_compile_options(picoprobe PRIVATE -Wall)

if (DEFINED ENV{PICOPROBE_LED})
        message("PICOPROBE_LED is defined as " $ENV{PICOPROBE_LED})
        target_compile_definitions(picoprobe PRIVATE PICOPROBE_LED=$ENV{PICOPROBE_LED})
endif()

set(DBG_PIN_COUNT=4)

target_include_directories(picoprobe PRIVATE src)

target_compile_definitions (picoprobe PRIVATE
	PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
)

target_link_libraries(picoprobe PRIVATE
        pico_multicore
        pico_stdlib
        pico_unique_id
        tinyusb_device
        tinyusb_board
)

pico_set_binary_type(picoprobe copy_to_ram)

pico_add_extra_outputs(picoprobe)
