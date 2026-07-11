@echo off
setlocal enabledelayedexpansion

set CUBEPROG=E:/ST/STM32/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe
set SCRIPT_DIR=%~dp0

echo.
echo ============================================
echo   W25Q128 SPI Flash Eraser
echo ============================================

if not exist "%SCRIPT_DIR%工具-SPIFlash全片擦除.hex" (
    echo [ERR] Cannot find erase firmware
    goto end
)

echo.
echo Flashing erase firmware to MCU...
"%CUBEPROG%" -c port=SWD freq=4000 -w "%SCRIPT_DIR%工具-SPIFlash全片擦除.hex" -v --rst
if errorlevel 1 (
    echo [ERR] Flash failed
    goto end
)

echo.
echo Erase started (~80 seconds).
echo Open serial monitor (115200) to see progress.
echo.
echo Then re-flash APP firmware to use.

:end
pause
