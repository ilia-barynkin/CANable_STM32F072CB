cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake -S . -B build/debug")
endif()

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake"
    CACHE FILEPATH "Arm bare-metal toolchain")
project(canable LANGUAGES C ASM)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CANABLE_MCU STM32F072CB CACHE STRING "Microcontroller")
set_property(CACHE CANABLE_MCU PROPERTY STRINGS STM32F072CB STM32F072C8)
if(CANABLE_MCU STREQUAL "STM32F072CB")
    set(CANABLE_FLASH_BYTES 131072)
elseif(CANABLE_MCU STREQUAL "STM32F072C8")
    set(CANABLE_FLASH_BYTES 65536)
else()
    message(FATAL_ERROR "Unknown CANABLE_MCU '${CANABLE_MCU}'; use STM32F072CB or STM32F072C8")
endif()

set(CANABLE_BOARD BLUEPILL CACHE STRING "Board wiring and LED configuration")
set(CANABLE_BOARDS BLUEPILL FYSETC P_CAN_07e CANable_V1_0_Pro Jhoinrch InnoMaker InnoMakerX2)
set(CANABLE_BOARD_DEFINES BLUEPILL FYSETC_UCAN P_CAN_07e CANABLE_V1_0_PRO JHOINRCH INNOMAKER_USB2CAN INNOMAKER_USB2CANX2)
set_property(CACHE CANABLE_BOARD PROPERTY STRINGS ${CANABLE_BOARDS})
list(FIND CANABLE_BOARDS "${CANABLE_BOARD}" CANABLE_BOARD_INDEX)
if(CANABLE_BOARD_INDEX EQUAL -1)
    message(FATAL_ERROR "Unknown CANABLE_BOARD '${CANABLE_BOARD}'; choose one of: ${CANABLE_BOARDS}")
endif()
list(GET CANABLE_BOARD_DEFINES ${CANABLE_BOARD_INDEX} CANABLE_BOARD_DEFINE)

set(CANABLE_USB_PROTOCOL GS_USB CACHE STRING "USB protocol: SocketCAN gs_usb or legacy CAN-X CDC")
set_property(CACHE CANABLE_USB_PROTOCOL PROPERTY STRINGS GS_USB CAN_X)
if(CANABLE_USB_PROTOCOL STREQUAL "GS_USB")
    set(CANABLE_PROTOCOL_SOURCES
        Core/Src/gs_can.c
        USB_DEVICE/App/usbd_gs_usb.c
    )
elseif(CANABLE_USB_PROTOCOL STREQUAL "CAN_X")
    set(CANABLE_PROTOCOL_SOURCES
        Core/Src/CAN_Buffer.c
        Core/Src/CAN_Filter.c
        Core/Src/CAN_Mode.c
        Core/Src/ErrorStatus.c
        Core/Src/IRQ_Handler.c
        Core/Src/PollingRoutine.c
        Core/Src/RingBuffer.c
        Core/Src/STM32_UID.c
        Core/Src/UART_DMA_Idle_Circular_Drv_STM32.c
        Core/Src/USB_CAN_Convert.c
        Core/Src/USB_CDC_Buffer.c
        Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c
        USB_DEVICE/App/usbd_cdc_if.c
    )
else()
    message(FATAL_ERROR "Unknown CANABLE_USB_PROTOCOL '${CANABLE_USB_PROTOCOL}'; use GS_USB or CAN_X")
endif()

add_executable(canable
    Core/Startup/startup_stm32f072c8tx.s
    ${CANABLE_PROTOCOL_SOURCES}
    Core/Src/GPIO_Handler.c
    Core/Src/TimerCallback.c
    Core/Src/main.c
    Core/Src/stm32f0xx_hal_msp.c
    Core/Src/stm32f0xx_it.c
    Core/Src/syscalls.c
    Core/Src/sysmem.c
    Core/Src/system_stm32f0xx.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_can.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_cortex.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_dma.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_exti.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_flash.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_flash_ex.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_gpio.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_i2c.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_i2c_ex.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pcd.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pcd_ex.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pwr.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pwr_ex.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_rcc.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_rcc_ex.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_uart.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_uart_ex.c
    Drivers/STM32F0xx_HAL_Driver/Src/stm32f0xx_ll_usb.c
    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
    USB_DEVICE/App/usb_device.c
    USB_DEVICE/App/usbd_desc.c
    USB_DEVICE/Target/usbd_conf.c
)

set_target_properties(canable PROPERTIES
    SUFFIX .elf
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS YES
)
target_include_directories(canable PRIVATE
    Core/Inc
    Drivers/STM32F0xx_HAL_Driver/Inc
    Drivers/STM32F0xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F0xx/Include
    Drivers/CMSIS/Include
    USB_DEVICE/App
    USB_DEVICE/Target
    Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
)
target_compile_definitions(canable PRIVATE
    USE_HAL_DRIVER STM32F072xB ${CANABLE_BOARD_DEFINE}
    $<$<STREQUAL:${CANABLE_USB_PROTOCOL},GS_USB>:CANABLE_GS_USB>
    $<$<CONFIG:Debug>:DEBUG>
)
set(CANABLE_CPU_OPTIONS -mcpu=cortex-m0 -mthumb -mfloat-abi=soft)
target_compile_options(canable PRIVATE
    ${CANABLE_CPU_OPTIONS}
    --specs=nano.specs
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-O0;-g3>
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:-Os>
)

# The startup/vector table and memory layout are shared by F072C8 and F072CB.
# The linker script keeps its original 64 KiB default for legacy IDE builds.
set(CANABLE_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32F072C8TX_FLASH.ld")
set(CANABLE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
if(CMAKE_CONFIGURATION_TYPES)
    string(APPEND CANABLE_OUTPUT_DIR "/$<CONFIG>")
endif()
target_link_options(canable PRIVATE
    ${CANABLE_CPU_OPTIONS}
    --specs=nano.specs --specs=nosys.specs
    "LINKER:--defsym=__flash_size__=${CANABLE_FLASH_BYTES}"
    "-T${CANABLE_LINKER_SCRIPT}"
    LINKER:--gc-sections
    LINKER:--print-memory-usage
    "LINKER:-Map=${CANABLE_OUTPUT_DIR}/canable.map"
)
target_link_libraries(canable PRIVATE m)
set_property(TARGET canable APPEND PROPERTY LINK_DEPENDS "${CANABLE_LINKER_SCRIPT}")

add_custom_command(TARGET canable POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:canable>" "${CANABLE_OUTPUT_DIR}/canable.bin"
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:canable>" "${CANABLE_OUTPUT_DIR}/canable.hex"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:canable>"
    BYPRODUCTS "${CANABLE_OUTPUT_DIR}/canable.bin" "${CANABLE_OUTPUT_DIR}/canable.hex" "${CANABLE_OUTPUT_DIR}/canable.map"
    COMMENT "Generating firmware images and reporting memory usage"
    VERBATIM
)

message(STATUS "CANable board: ${CANABLE_BOARD}; MCU: ${CANABLE_MCU}; USB: ${CANABLE_USB_PROTOCOL}; Flash: ${CANABLE_FLASH_BYTES} bytes; RAM: 16384 bytes")
