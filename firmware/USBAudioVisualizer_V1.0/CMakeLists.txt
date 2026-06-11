cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME USBAudioVisualizer_V1.0)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    "Core/Src/oled.c"
    "Core/Src/logo_bitmap.c"
    "Core/Src/audio_dsp.c"
    "Core/Src/ws2812.c"
    # CMSIS-DSP library
    "Drivers/CMSIS/DSP/Source/arm_rfft_fast_f32.c"
    "Drivers/CMSIS/DSP/Source/arm_rfft_fast_init_f32.c"
    "Drivers/CMSIS/DSP/Source/arm_cfft_f32.c"
    "Drivers/CMSIS/DSP/Source/arm_cfft_init_f32.c"
    "Drivers/CMSIS/DSP/Source/arm_cfft_radix8_f32.c"
    "Drivers/CMSIS/DSP/Source/arm_bitreversal2.c"
    "Drivers/CMSIS/DSP/Source/arm_common_tables.c"
    "Drivers/CMSIS/DSP/Source/arm_const_structs.c"
    "Drivers/CMSIS/DSP/Source/arm_hanning_f32.c"
    "Drivers/CMSIS/DSP/Source/arm_cmplx_mag_f32.c"
    "Drivers/CMSIS/DSP/Source/arm_max_no_idx_f32.c"
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    "Drivers/CMSIS/DSP/Include"
    "Drivers/CMSIS/DSP/PrivateInclude"
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    ARM_MATH_DSP
    ARM_MATH_LOOPUNROLL
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Compile audio_dsp.c with -O2 for filterbank performance
set_source_files_properties(Core/Src/audio_dsp.c PROPERTIES COMPILE_FLAGS "-O2")

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
