@echo off
setlocal enabledelayedexpansion

set GCC=E:\ST\STM32\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\arm-none-eabi-gcc.exe
set OBJCOPY=E:\ST\STM32\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\arm-none-eabi-objcopy.exe
set CUBEPROG=E:\ST\STM32\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe
set CFLAGS=-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -Os -nostdlib -ffreestanding
set LDFLAGS=-Wl,-z,max-page-size=0x4 -specs=nosys.specs -T stm32f407_batch.ld

for /L %%B in (0,1,12) do (
    if %%B LSS 10 (set B2=0%%B) else (set B2=%%B)

    echo.
    echo === BATCH !B2! / 12 ===

    copy /Y batch_!B2!.h batch.h >nul
    if errorlevel 1 (echo NOT FOUND: batch_!B2!.h && goto end)

    echo Compiling...
    %GCC% %CFLAGS% main_batch.c -o batch_!B2!.elf %LDFLAGS%
    if errorlevel 1 (echo COMPILE FAIL && goto end)
    %OBJCOPY% -O ihex batch_!B2!.elf batch_!B2!.hex

    echo Flashing...
    "%CUBEPROG%" -c port=SWD freq=4000 -w batch_!B2!.hex -v
    if errorlevel 1 (echo FLASH FAIL && goto end)

    echo Flashed -- PRESS RESET ON BOARD, then press any key...
    pause >nul
)
echo.
echo ALL 13 BATCHES DONE! Flash main firmware now.
:end
pause
