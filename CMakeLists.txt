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
# LVGL v8.3 图形库 (uint3code 例程配套)
# ============================================================
file(GLOB_RECURSE LVGL_SOURCES
    lvgl-8.3.0/src/*.c
)
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/examples/")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/tests/")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/demos/")

add_library(lvgl OBJECT ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC
    Core/Inc
    Core/Inc/generated
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include
    lvgl-8.3.0
    lvgl-8.3.0/src
)
target_compile_definitions(lvgl PRIVATE
    USE_HAL_DRIVER
    STM32F407xx
    LV_CONF_INCLUDE_SIMPLE
)

# ============================================================
# GUI 已切换到 LVGL 8.3 — 废弃 GUI-Guider 生成代码
# guider 库及相关文件已移除。如需 UI 请使用 LVGL 8.3 API 手写。
# ============================================================

# ============================================================
# 项目源文件
# ============================================================
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/en25q128.c
    Core/Src/nt35510.c
    Core/Src/lv_port_disp.c
    Core/Src/lv_fs_spi_flash.c
    Core/Src/spiflash_prog.c
    Core/Src/inter_flash_cfg.c
    Core/Src/inter_flashif.c
    Core/Src/hw_diag.c
    Core/Src/touch/ctiic.c
    Core/Src/touch/gt5663.c
    Core/Src/touch/touch.c
    BSP/Canif/canif.c

    # APP 精简 GUI
    Core/Src/app_ui.c

    # LVGL 图片资源（来自 uint3code 例程）
    Core/Src/images/ui_img_942215904.c
    Core/Src/images/ui_img_1601502596.c
    Core/Src/images/ui_img_light_png.c
    Core/Src/images/ui_img_safety_belt_png.c
    Core/Src/images/ui_img_temp_gray_png.c
    Core/Src/images/ui_img_turn_light_png.c
    # 电机/舵机驱动已移除 — 电源限制导致三方案均失败
    # BSP/Driver/DRV8833.c
    # BSP/Driver/SG-90.c

)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Inc/touch
    BSP/Canif
)

# Ensure LVGL config is found (inherited by all source files)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    LV_CONF_INCLUDE_SIMPLE
)

# ── APP 分区配置 ──
# 1. 链接脚本: APP 从 0x08008000 开始
# 2. VTOR: 启动时系统自动将向量表重定位到 0x08008000
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T "${CMAKE_SOURCE_DIR}/STM32F407XX_FLASH_APP.ld"
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USER_VECT_TAB_ADDRESS
    VECT_TAB_OFFSET=0x8000
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

    # Add user defined libraries
)

# ============================================================
# Bootloader Target — 最小化裸机跳转器
#   Phase 1: 初始化时钟 + GPIO + USART1 + CAN1 → 跳转 APP
#   不含: FreeRTOS, LVGL, SPI Flash, Touch, LCD, FreeRTOS
# ============================================================
add_executable(bootloader)

target_sources(bootloader PRIVATE
    # Bootloader 入口 + 跳转逻辑
    Bootloader/Src/main.c
    Core/Src/boot_manager.c

    # 系统启动（不含 main.c / freertos.c — 那是 APP 的）
    # system_stm32f4xx.c 已在 STM32_Drivers 中，这里不重复添加
    Core/Src/syscalls.c
    Core/Src/sysmem.c
    startup_stm32f407xx.s

    # CubeMX 外设初始化（最小集）
    Core/Src/gpio.c
    Core/Src/can.c
    Core/Src/usart.c
    Core/Src/stm32f4xx_it.c
    Core/Src/stm32f4xx_hal_msp.c
    Core/Src/stm32f4xx_hal_timebase_tim.c
    Core/Src/inter_flash_cfg.c
    Core/Src/inter_flashif.c
    Bootloader/Src/ymodem.c
    Bootloader/Src/ymodem_porting.c
)

target_include_directories(bootloader PRIVATE
    Bootloader/Inc
)

target_link_libraries(bootloader PRIVATE
    stm32cubemx
    STM32_Drivers
)

# Bootloader 链接脚本 + map
target_link_options(bootloader PRIVATE
    -T "${CMAKE_SOURCE_DIR}/STM32F407XX_FLASH_BOOT.ld"
    -Wl,-Map=bootloader.map
)

# Bootloader 不需要 VTOR 偏移（从 0x08000000 启动）
# 不需要 LV_CONF_INCLUDE_SIMPLE（不含 LVGL）

# ── 生成 .bin 文件 ──
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin"
    COMMENT "Generating APP .bin..."
)
add_custom_command(TARGET bootloader POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bootloader>
            "${CMAKE_BINARY_DIR}/bootloader.bin"
    COMMENT "Generating Bootloader .bin..."
)
