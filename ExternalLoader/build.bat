@echo off
:: External Loader 编译脚本 — W25Q128 for STM32F407
:: 产物: W25Q128_STM32F407.stldr → 复制到 CubeProgrammer ExternalLoader 目录

set TOOLCHAIN=E:\ST\STM32\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin
set GCC=%TOOLCHAIN%\arm-none-eabi-gcc.exe
set OBJCOPY=%TOOLCHAIN%\arm-none-eabi-objcopy.exe

set CFLAGS=-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Os -nostdlib -ffreestanding
set LDFLAGS=-T stm32f407_loader.ld -nostdlib -Wl,-Map=loader.map

echo.
echo === Compiling W25Q128 External Loader ===

echo [1/2] Compiling...
%GCC% %CFLAGS% -c dev_inf.c -o dev_inf.o
if errorlevel 1 goto err
%GCC% %CFLAGS% -c loader.c -o loader.o
if errorlevel 1 goto err

echo [2/2] Linking STM32F407...
%GCC% %CFLAGS% %LDFLAGS% dev_inf.o loader.o -o loader.elf
if errorlevel 1 goto err

echo Stripping...
%OBJCOPY% -O binary loader.elf loader.bin

:: 重命名为 .stldr
copy /Y loader.elf W25Q128_STM32F407.stldr >nul

echo.
echo ============================================
echo   SUCCESS: W25Q128_STM32F407.stldr
echo ============================================
echo.
echo   Next: copy to CubeProgrammer ExternalLoader dir
echo.
goto end

:err
echo.
echo   BUILD FAILED
:end
