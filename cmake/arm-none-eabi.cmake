set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Compiler checks cannot link a bare-metal program before the target supplies
# its startup code and linker script.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(ARM_NONE_EABI_GCC NAMES arm-none-eabi-gcc)
if(NOT ARM_NONE_EABI_GCC)
    message(FATAL_ERROR
        "arm-none-eabi-gcc was not found. Install the GNU Arm Embedded "
        "toolchain and add its bin directory to PATH.")
endif()

set(CMAKE_C_COMPILER "${ARM_NONE_EABI_GCC}")
set(CMAKE_ASM_COMPILER "${ARM_NONE_EABI_GCC}")
get_filename_component(_arm_toolchain_bin "${ARM_NONE_EABI_GCC}" DIRECTORY)

foreach(_tool IN ITEMS ar ranlib objcopy size)
    string(TOUPPER "${_tool}" _tool_upper)
    find_program(CMAKE_${_tool_upper}
        NAMES arm-none-eabi-${_tool}
        HINTS "${_arm_toolchain_bin}")
    if(NOT CMAKE_${_tool_upper})
        message(FATAL_ERROR
            "arm-none-eabi-${_tool} was not found. Install the complete GNU "
            "Arm Embedded toolchain and add its bin directory to PATH.")
    endif()
endforeach()

# Build tools run on the host; headers and libraries belong to the target.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
