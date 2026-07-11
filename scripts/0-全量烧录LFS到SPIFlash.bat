@echo off
setlocal enabledelayedexpansion

set GCC=E:/ST/STM32/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin/arm-none-eabi-gcc.exe
set OBJCOPY=E:/ST/STM32/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin/arm-none-eabi-objcopy.exe
set CUBEPROG=E:/ST/STM32/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe
set CFLAGS=-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -Os -nostdlib -ffreestanding -Wl,--gc-sections
set LDFLAGS=-T stm32f407_batch.ld

set SCRIPT_DIR=%~dp0

echo =============================================
echo LFS Batch Programmer - LittleFS full flash
echo =============================================

REM ---- Step 0: Check lfs_img.bin ----
if not exist "%SCRIPT_DIR%lfs_img.bin" (
    echo [ERR] lfs_img.bin not found
    echo       Run 工具-从图片生成LFS镜像.py first
    goto end
)

REM ---- Step 1: Generate batch headers ----
echo.
echo [1/4] Generating batch_XX.h headers...
python "%SCRIPT_DIR%工具-拆分LFS镜像为批次头文件.py"
if errorlevel 1 (
    echo [ERR] Failed to generate batch headers
    goto end
)

REM ---- Step 2: Full chip erase ----
echo.
echo [2/4] Full chip erase (W25Q128, ~80s)...

if not exist "%SCRIPT_DIR%工具-SPIFlash全片擦除.hex" (
    echo [WARN] Erase firmware not found, skipping erase
) else (
    echo  Flashing erase firmware...
    "%CUBEPROG%" -c port=SWD freq=4000 -w "%SCRIPT_DIR%工具-SPIFlash全片擦除.hex" -v --rst
    if errorlevel 1 (
        echo [ERR] Erase flash failed
        goto end
    )
    echo  Waiting for erase to complete...
    echo  Press any key after serial shows ERASE DONE
    pause >nul
)

REM ---- Step 3: Batch flash ----
echo.
echo [3/4] Flashing LittleFS image in batches...

REM Find max batch number
set MAX_BATCH=0
for %%f in ("%SCRIPT_DIR%batch_*.h") do (
    set FNAME=%%~nf
    set NUM=!FNAME:batch_=!
    echo !NUM!|findstr /R "^[0-9][0-9]*$" >nul && (
        if !NUM! gtr !MAX_BATCH! set MAX_BATCH=!NUM!
    )
)
echo  Detected !MAX_BATCH! batches (batch_00.h ~ batch_!MAX_BATCH!.h)

for /L %%i in (0,1,!MAX_BATCH!) do (
    set NUM=%%i
    if %%i LSS 10 set NUM=0%%i

    echo.
    echo  --- Batch !NUM! / !MAX_BATCH! ---

    copy /Y "%SCRIPT_DIR%batch_!NUM!.h" "%SCRIPT_DIR%batch.h" >nul 2>&1
    if errorlevel 1 (
        echo  [WARN] batch_!NUM!.h missing, skip
        goto next_batch
    )

    echo  Compiling...
    pushd "%SCRIPT_DIR%"
    %GCC% %CFLAGS% 固件-全量烧录.c -o batch_!NUM!.elf %LDFLAGS%
    if errorlevel 1 (echo  [ERR] Compile failed && popd && goto end)
    %OBJCOPY% -O ihex batch_!NUM!.elf batch_!NUM!.hex
    popd

    echo  Flashing...
    "%CUBEPROG%" -c port=SWD freq=4000 -w "%SCRIPT_DIR%batch_!NUM!.hex" -v --rst
    if errorlevel 1 (
        echo  [WARN] Batch !NUM! flash failed, continuing
    ) else (
        echo  Batch !NUM! OK
    )
)

:next_batch

REM ---- Step 4: Cleanup ----
echo.
echo [4/4] Cleaning up temporary files...
del "%SCRIPT_DIR%batch.h" 2>nul

echo.
echo =============================================
echo LittleFS image flash complete!
echo =============================================
echo.
echo Next: flash Bootloader + APP firmware via OpenOCD

:end
pause
