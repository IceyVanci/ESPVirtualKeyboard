@echo off
title ESP Virtual Keyboard - Firmware Updater

echo ============================================================
echo   ESP Virtual Keyboard - Firmware Update Tool
echo ============================================================
echo.
echo  Starting local server on http://127.0.0.1:8123 ...
echo  A browser window will open automatically.
echo.
echo  IMPORTANT:
echo    - Keep this window OPEN while upgrading.
echo    - Use Chrome or Edge browser (Web Serial required).
echo    - Do NOT disconnect USB or power off during upgrade.
echo    - Closing this window stops the tool.
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0server.ps1"