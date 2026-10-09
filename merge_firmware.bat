@echo off
setlocal enabledelayedexpansion

echo ============================================================
echo          Agar Multi-Device Firmware Merging Tool
echo ============================================================

set "PIO_PY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
set "ESPTOOL_PY=%USERPROFILE%\.platformio\packages\tool-esptoolpy\esptool.py"
set "BOOT_APP0=%USERPROFILE%\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"

if not exist "%BOOT_APP0%" (
    echo [ERROR] boot_app0.bin not found at: %BOOT_APP0%
    pause
    exit /b 1
)

where esptool >nul 2>nul
if %errorlevel% equ 0 (
    set "ESPTOOL_CMD=esptool"
) else (
    set "ESPTOOL_CMD="%PIO_PY%" "%ESPTOOL_PY%""
)

set "PARAM=%~1"
if not "%PARAM%"=="" goto handle_param

:menu
echo.
echo Please select target device:
echo   [1] M5Stack StopWatch  (16MB Flash, 466x466 AMOLED)
echo   [2] M5StickS3          (8MB Flash)
echo   [3] M5Cardputer        (8MB Flash)
echo   [4] Build All Devices
echo   [0] Exit
echo.
set /p "CHOICE=Enter choice [1-4] (default 1): "
if "%CHOICE%"=="" set "CHOICE=1"

if "%CHOICE%"=="1" goto run_stopwatch
if "%CHOICE%"=="2" goto run_sticks3
if "%CHOICE%"=="3" goto run_cardputer
if "%CHOICE%"=="4" goto run_all
if "%CHOICE%"=="0" goto end
echo Invalid option, please try again.
goto menu

:handle_param
if /i "%PARAM%"=="stopwatch" goto run_stopwatch
if /i "%PARAM%"=="sticks3" goto run_sticks3
if /i "%PARAM%"=="cardputer" goto run_cardputer
if /i "%PARAM%"=="all" goto run_all
if "%PARAM%"=="1" goto run_stopwatch
if "%PARAM%"=="2" goto run_sticks3
if "%PARAM%"=="3" goto run_cardputer
if "%PARAM%"=="4" goto run_all
echo Unknown parameter: %PARAM%
goto menu

:run_stopwatch
call :do_merge "m5stack-stopwatch" "16MB" "agar-stopwatch-factory.bin" "M5Stack StopWatch"
goto finish

:run_sticks3
call :do_merge "m5stack-sticks3" "8MB" "agar-sticks3-factory.bin" "M5StickS3"
goto finish

:run_cardputer
call :do_merge "m5stack-cardputer" "8MB" "cardputer-factory.bin" "M5Cardputer"
goto finish

:run_all
call :do_merge "m5stack-stopwatch" "16MB" "agar-stopwatch-factory.bin" "M5Stack StopWatch"
call :do_merge "m5stack-sticks3" "8MB" "agar-sticks3-factory.bin" "M5StickS3"
call :do_merge "m5stack-cardputer" "8MB" "cardputer-factory.bin" "M5Cardputer"
goto finish

:finish
echo.
echo ============================================================
echo               All tasks completed!
echo ============================================================
if "%~1"=="" pause
goto end

:do_merge
set "ENV=%~1"
set "FSIZE=%~2"
set "OUT=%~3"
set "DEV=%~4"

echo.
echo ------------------------------------------------------------
echo Processing: %DEV% (%ENV%)
echo ------------------------------------------------------------

set "BUILD_DIR=.pio\build\%ENV%"
if not exist "%BUILD_DIR%\firmware.bin" (
    echo [INFO] firmware.bin not found in %BUILD_DIR%
    echo Building %ENV% using PlatformIO...
    where pio >nul 2>nul
    if !errorlevel! equ 0 (
        pio run -e %ENV%
    ) else (
        "%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" run -e %ENV%
    )
)

if not exist "%BUILD_DIR%\firmware.bin" (
    echo [ERROR] Build failed or firmware.bin still missing for %ENV%.
    exit /b 1
)

echo Merging firmware binaries to single factory image...
%ESPTOOL_CMD% --chip esp32s3 merge_bin -o "%OUT%" --flash_mode dio --flash_size %FSIZE% 0x0000 "%BUILD_DIR%\bootloader.bin" 0x8000 "%BUILD_DIR%\partitions.bin" 0xe000 "%BOOT_APP0%" 0x10000 "%BUILD_DIR%\firmware.bin"

if !errorlevel! equ 0 (
    echo [SUCCESS] %DEV% firmware merged into: %OUT%
    for %%F in ("%OUT%") do echo           File Size: %%~zF bytes
    echo           Flash Command: esptool --chip esp32s3 write_flash 0x0 %OUT%
) else (
    echo [FAILED] merge_bin failed with error code !errorlevel!
)
exit /b 0

:end
endlocal
