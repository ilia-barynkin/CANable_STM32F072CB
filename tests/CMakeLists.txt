cmake_minimum_required(VERSION 3.20)
project(canable_host_tests C)

enable_testing()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Native compiler only: exercise the production USB class and ST core using
# their actual public types. Only hardware calls and the CAN backend are fake.
add_executable(usbd_gs_usb_test
    test_usbd_gs_usb.c
    ../USB_DEVICE/App/usbd_gs_usb.c
    ../Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
)
target_include_directories(usbd_gs_usb_test PRIVATE
    stubs
    ../Core/Inc
    ../USB_DEVICE/App
    ../Middlewares/ST/STM32_USB_Device_Library/Core/Inc
)
target_compile_definitions(usbd_gs_usb_test PRIVATE CANABLE_GS_USB)
target_compile_options(usbd_gs_usb_test PRIVATE
    -Wall -Wextra -Werror -ffunction-sections -fdata-sections
)
# Drop unused ST core entry points, whose platform dependencies are irrelevant
# to the control-transfer, reset and disconnect paths under test.
target_link_options(usbd_gs_usb_test PRIVATE -Wl,--gc-sections)
add_test(NAME usbd_gs_usb COMMAND usbd_gs_usb_test)

# Exercise the production queue/controller logic with deterministic CAN
# registers and HAL calls; no device or cross compiler is needed.
add_executable(gs_can_test
    can_backend/test_gs_can.c
    ../Core/Src/gs_can.c
)
target_include_directories(gs_can_test PRIVATE can_backend/stubs ../Core/Inc)
target_compile_definitions(gs_can_test PRIVATE CANABLE_GS_USB)
target_compile_options(gs_can_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME gs_can COMMAND gs_can_test)
