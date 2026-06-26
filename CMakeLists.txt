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

# SPI Flash batch programming mode
option(BATCH_FLASH "Build as SPI Flash batch programmer" OFF)

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
# LVGL v9.3 图形库 (GUI-Guider 配套)
# ============================================================
file(GLOB_RECURSE LVGL_SOURCES
    lvgl-9.3.0/src/*.c
)
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/examples/")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/tests/")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/demos/")

add_library(lvgl OBJECT ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC
    lvgl-9.3.0
    lvgl-9.3.0/src
    Core/Inc
    Core/Inc/generated
)
target_compile_definitions(lvgl PRIVATE
    USE_HAL_DRIVER
    STM32F407xx
    LV_CONF_INCLUDE_SIMPLE
)

# ============================================================
# GUI-Guider 生成代码 (仅核心 UI 结构, 图片/字体放 SPI Flash)
# ============================================================
file(GLOB_RECURSE GUIDER_SOURCES
    Core/Src/generated/gui_guider.c
    Core/Src/generated/setup_scr_home.c
    Core/Src/generated/setup_scr_mode.c
    Core/Src/generated/events_init.c
    Core/Src/generated/widgets_init.c
)
add_library(guider OBJECT ${GUIDER_SOURCES})
target_include_directories(guider PUBLIC
    Core/Inc
    Core/Inc/generated
)
target_link_libraries(guider PUBLIC lvgl)

# ============================================================
# 项目源文件
# ============================================================
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/en25q128.c
    Core/Src/nt35510.c
    Core/Src/lv_port_disp.c
    Core/Src/lv_fs_spi_flash.c
    Core/Src/spiflash_images.c
    Core/Src/spiflash_prog.c
    Core/Src/custom.c
    Core/Src/touch/ctiic.c
    Core/Src/touch/gt5663.c
    Core/Src/touch/touch.c
    BSP/Canif/canif.c
    Core/Src/generated/guider_fonts/lv_font_LiberationSans_53.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Inc/touch
    BSP/Canif
    Core/Inc/generated/guider_customer_fonts
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

if(BATCH_FLASH)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE BATCH_FLASH=1)
endif()

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    lvgl
    guider

    # Add user defined libraries
)
