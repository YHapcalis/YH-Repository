@echo off
:: ============================================
::  W25Q128 全片擦除工具
::  1. 编译 erase_spi.c
::  2. 烧录到 STM32 内部 Flash
::  3. MCU 自动执行全片擦除
::  4. UART 监控输出 "ERASE DONE"
:: ============================================
setlocal enabledelayedexpansion

set GCC=E:\ST\STM32\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\arm-none-eabi-gcc.exe
set OBJCOPY=E:\ST\STM32\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\arm-none-eabi-objcopy.exe
set CUBEPROG=E:\ST\STM32\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe
set CFLAGS=-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -Os -nostdlib -ffreestanding
set LDFLAGS=-Wl,-z,max-page-size=0x4 -specs=nosys.specs -T stm32f407_batch.ld

echo.
echo ============================================
echo   W25Q128 SPI Flash Eraser
echo ============================================
echo.
echo [1/3] Compiling erase_spi.c ...
%GCC% %CFLAGS% erase_spi.c -o erase_spi.elf %LDFLAGS%
if errorlevel 1 (echo COMPILE FAIL && goto end)
%OBJCOPY% -O ihex erase_spi.elf erase_spi.hex
echo   OK

echo [2/3] Flashing via ST-LINK ...
"%CUBEPROG%" -c port=SWD freq=4000 -w erase_spi.hex -v -rst
if errorlevel 1 (echo FLASH FAIL && goto end)
echo   OK

echo.
echo [3/3] MCU is now erasing W25Q128 ...
echo        Watch UART output (115200-8N1)
echo        LED ON = erasing, LED OFF = done
echo        Full chip erase takes ~40-80 seconds
echo.
echo ============================================
echo   Open serial monitor and wait for:
echo   ">>> FINISHED — SPI Flash is now all 0xFF"
echo ============================================

:end
pause
