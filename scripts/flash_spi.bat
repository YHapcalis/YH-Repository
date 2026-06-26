@echo off
:: ============================================
::  W25Q128 SPI Flash 一键烧录
::  1. 提取 C 数组 → spiflash.bin
::  2. STM32CubeProgrammer CLI 烧录
:: ============================================

setlocal enabledelayedexpansion

echo.
echo ============================================
echo   SPI Flash 图片烧录工具
echo ============================================
echo.

:: ---- Step 1: 生成二进制 ----
echo [1/3] Generating spiflash.bin...
D:\Python\python.exe make_spiflash_bin.py ..\Core\Src\generated\images spiflash.bin
if errorlevel 1 goto err

:: ---- Step 2: SPI Flash 烧录 ----
echo.
echo [2/3] Flashing SPI Flash (ST-LINK)...
set CUBEPROG=E:\ST\STM32\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.400.202601091506\tools\bin
set LOADER=E:\ST\STM32\MY_workspace\Projects_F407\MY_OTA_GUI\ExternalLoader\W25Q128_STM32F407.stldr

"%CUBEPROG%\STM32_Programmer_CLI.exe" -c port=SWD freq=4000 -el "%LOADER%" -w spiflash.bin 0x00000000 -v
if errorlevel 1 (
    echo.
    echo FLASH FAILED. 检查 ST-LINK 连接和 loader 路径.
    goto end
)

echo.
echo [3/3] Done!
echo ============================================
echo   SUCCESS: SPI Flash 烧录完成
echo ============================================
goto end

:err
echo FAILED at step 1. 检查 Python 和图片目录.
:end
pause
