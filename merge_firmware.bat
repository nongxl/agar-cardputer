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
echo 请选择需要打包的目标设备 (Please select target device):
echo   [1] M5Stack StopWatch  (16MB Flash, 466x466 AMOLED)
echo   [2] M5StickS3          (8MB Flash)
echo   [3] M5Cardputer        (8MB Flash)
echo   [4] 全部打包 (Build All)
echo   [0] 退出 (Exit)
echo.
set /p "CHOICE=请输入选项序号 [1-4] (默认 1): "
if "%CHOICE%"=="" set "CHOICE=1"

if "%CHOICE%"=="1" goto run_stopwatch
if "%CHOICE%"=="2" goto run_sticks3
if "%CHOICE%"=="3" goto run_cardputer
if "%CHOICE%"=="4" goto run_all
if "%CHOICE%"=="0" goto end
echo 无效选项，请重新输入。
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
echo 未知参数: %PARAM%
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
echo 所有选定任务处理完毕！
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
echo 正在处理: %DEV% (%ENV%)
echo ------------------------------------------------------------

set "BUILD_DIR=.pio\build\%ENV%"
if not exist "%BUILD_DIR%\firmware.bin" (
    echo [提示] 未找到编译产物: %BUILD_DIR%\firmware.bin
    echo 正在使用 platformio 编译 %ENV% ...
    where pio >nul 2>nul
    if !errorlevel! equ 0 (
        pio run -e %ENV%
    ) else (
        "%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" run -e %ENV%
    )
)

if not exist "%BUILD_DIR%\firmware.bin" (
    echo [错误] 编译未完成或未生成 firmware.bin: %ENV%
    exit /b 1
)

echo 执行 merge_bin 生成单文件固件...
%ESPTOOL_CMD% --chip esp32s3 merge_bin -o "%OUT%" --flash_mode dio --flash_size %FSIZE% 0x0000 "%BUILD_DIR%\bootloader.bin" 0x8000 "%BUILD_DIR%\partitions.bin" 0xe000 "%BOOT_APP0%" 0x10000 "%BUILD_DIR%\firmware.bin"

if !errorlevel! equ 0 (
    echo [成功] %DEV% 固件已合并至: %OUT%
    for %%F in ("%OUT%") do echo        文件大小: %%~zF 字节
    echo        烧录命令: esptool --chip esp32s3 write_flash 0x0 %OUT%
) else (
    echo [失败] merge_bin 出错，错误码: !errorlevel!
)
exit /b 0

:end
endlocal
