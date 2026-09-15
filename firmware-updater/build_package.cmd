@echo off
setlocal enabledelayedexpansion
title ESP Virtual Keyboard - Build Firmware Package

set "SCRIPT_DIR=%~dp0"
set "FW_DIR=%SCRIPT_DIR%firmware"
set "BUILD_DIR=%SCRIPT_DIR%build_tmp"
set "PUBLIC_DIR=%SCRIPT_DIR%public"
set "SKETCH_DIR=F:\ESPVirtualKeyboard"
set "CORE_DIR=C:\Users\16454\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.11"
set "FQBN=esp32:esp32:esp32c3:UploadSpeed=921600,CDCOnBoot=cdc,CPUFreq=160,FlashFreq=80,FlashMode=dio,FlashSize=4M,PartitionScheme=huge_app,DebugLevel=none,EraseFlash=none"
set "SRC=%BUILD_DIR%\ESPVirtualKeyboard"

echo ============================================================
echo  ESP Virtual Keyboard - Build Firmware Package
echo  Single firmware / FORCED EMPTY WiFi credentials
echo ============================================================
echo.

rem ---------- cleanup old outputs ----------
if exist "%FW_DIR%" rd /s /q "%FW_DIR%"
if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
mkdir "%FW_DIR%"

rem ---------- copy source files (.ino/.h/.cpp/.c) into sketch copy ----------
if not exist "%SRC%" mkdir "%SRC%"
copy /Y "%SKETCH_DIR%\*.ino"   "%SRC%\" >nul 2>&1 || goto :copyfail
copy /Y "%SKETCH_DIR%\*.h"     "%SRC%\" >nul 2>&1 || goto :copyfail
copy /Y "%SKETCH_DIR%\*.cpp"   "%SRC%\" >nul 2>&1 || goto :copyfail
rem *.c is optional (root currently has none; copies if present without failing)
copy /Y "%SKETCH_DIR%\*.c"     "%SRC%\" >nul 2>&1

rem ---------- FORCE EMPTY WiFi credentials in config.h copy (D13, no --with-creds) ----------
echo [INFO] forcing empty WiFi credentials in config.h copy...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='%SRC%\config.h'; $t=[System.IO.File]::ReadAllText($p,[System.Text.Encoding]::UTF8); $q=[char]34; $n=$t -replace '(?m)^\s*#define\s+WIFI_SSID\s+.*$', ('#define WIFI_SSID     '+$q+$q) -replace '(?m)^\s*#define\s+WIFI_PASSWORD\s+.*$', ('#define WIFI_PASSWORD '+$q+$q); if($n -eq $t){ Write-Error 'WIFI_SSID/WIFI_PASSWORD lines not found in config.h'; exit 1 }; [System.IO.File]::WriteAllText($p,$n,(New-Object System.Text.UTF8Encoding($false)))"
if errorlevel 1 (
    echo [ERROR] Failed to force empty credentials in config.h copy.
    exit /b 1
)
echo [INFO] credentials: EMPTY (forced)

rem ---------- compile ----------
echo [Compiling firmware (huge_app, may take 1-2 minutes)...]
arduino-cli compile --fqbn "%FQBN%" --output-dir "%FW_DIR%" "%SRC%"
if errorlevel 1 exit /b 1

rem ---------- copy firmware images ----------
echo [Copying firmware images...]
copy /Y "%FW_DIR%\ESPVirtualKeyboard.ino.bootloader.bin" "%FW_DIR%\bootloader.bin" >nul || goto :copyfail
copy /Y "%FW_DIR%\ESPVirtualKeyboard.ino.partitions.bin" "%FW_DIR%\partitions.bin"  >nul || goto :copyfail
copy /Y "%FW_DIR%\ESPVirtualKeyboard.ino.bin"            "%FW_DIR%\app.bin"         >nul || goto :copyfail
copy /Y "%CORE_DIR%\tools\partitions\boot_app0.bin"      "%FW_DIR%\boot_app0.bin"   >nul || goto :copyfail
echo       bootloader.bin / partitions.bin / boot_app0.bin / app.bin - OK
del /q "%FW_DIR%\ESPVirtualKeyboard.ino.*" >nul 2>&1

rem ---------- manifest + zip (asserts empty credentials on the COPY) ----------
echo [Generating manifest, packaging zip, checking bundle...]
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%gen_manifest.ps1" -FwDir "%FW_DIR%" -ConfigH "%SRC%\config.h" -PublicDir "%PUBLIC_DIR%"
if errorlevel 1 (
    echo [ERROR] Manifest generation failed (empty-credential assertion or packaging error).
    exit /b 1
)
echo.
echo ============================================================
echo  Package build finished:
echo    %FW_DIR%
echo ============================================================
exit /b 0

:copyfail
echo [ERROR] Copying files failed.
exit /b 1