@echo off
setlocal enabledelayedexpansion
title ESP Virtual Keyboard - CLI Flash Tool

set "TOOLDIR=%~dp0"
set "ESPTOOL=%TOOLDIR%esptool.exe"
set "FW=%TOOLDIR%firmware"

rem ---- erase option: default erase-all, --no-erase keeps config ----
set "ERASE_ARG=--erase-all"
set "ERASE_MSG=This will ERASE ALL flash (config will be cleared, need to reprovision WiFi)."
if /i "%~1"=="--no-erase" (
    set "ERASE_ARG="
    set "ERASE_MSG=Keeping existing config (NVS preserved)."
)

echo ============================================================
echo   ESP Virtual Keyboard - Firmware Flash (CLI fallback)
echo   %ERASE_MSG%
echo ============================================================
echo.

if not exist "%FW%\app.bin" (
    echo [ERROR] firmware\app.bin not found.
    echo         Run build_package.cmd first to build the firmware package.
    pause
    exit /b 1
)

echo Detecting ESP32-C3 serial port...
set "COMPORT="
for /f "delims=" %%P in ('powershell -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames()"') do (
    echo   probing %%P ...
    "%ESPTOOL%" --chip esp32c3 --port %%P --baud 115200 --connect-attempts 2 read-mac >nul 2>&1
    if not errorlevel 1 (
        set "COMPORT=%%P"
        goto :found
    )
)

echo.
echo [ERROR] No ESP32-C3 found on any COM port.
echo         - Check USB cable and connection.
echo         - Close any other program using the port, then retry.
pause
exit /b 1

:found
echo.
echo Found ESP32-C3 on %COMPORT%
echo %ERASE_MSG%
echo Flashing in 3 seconds - do NOT disconnect...
timeout /t 3 /nobreak >nul
"%ESPTOOL%" --chip esp32c3 --port %COMPORT% --baud 921600 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-freq 80m --flash-size 4MB %ERASE_ARG% 0x0 "%FW%\bootloader.bin" 0x8000 "%FW%\partitions.bin" 0xe000 "%FW%\boot_app0.bin" 0x10000 "%FW%\app.bin"
if errorlevel 1 (
    echo.
    echo [ERROR] Flash failed. Retry: reconnect USB and run this script again.
    pause
    exit /b 1
)
echo.
echo ============================================================
echo   Flash OK - device is restarting with new firmware.
echo   Note: WiFi provisioning is NOT available from CLI.
echo         Use the web tool "provision only" for WiFi setup.
echo ============================================================
pause