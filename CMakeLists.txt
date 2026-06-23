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
set(CMAKE_PROJECT_NAME MY_OTA_GUI)

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

# ============================================================
# LVGL v9.5 图形库
# ============================================================
# 源码文件 (排除 examples, tests, demos)
file(GLOB_RECURSE LVGL_SOURCES
    lvgl-9.5.0源码/src/*.c
)
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/examples/")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/tests/")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/demos/")

# LVGL 子库 (作为 OBJECT 库避免重复编译)
add_library(lvgl OBJECT ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC
    lvgl-9.5.0源码
    lvgl-9.5.0源码/src
    Core/Inc
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    Middlewares/Third_Party/FreeRTOS/Source/include
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
)
target_compile_definitions(lvgl PRIVATE
    USE_HAL_DRIVER
    STM32F407xx
    LV_CONF_INCLUDE_SIMPLE
)

# ============================================================
# 项目源文件
# ============================================================
# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/en25q128.c
    Core/Src/nt35510.c
    Core/Src/lv_port_disp.c
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    lvgl

    # Add user defined libraries
)
