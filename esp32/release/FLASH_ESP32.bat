@echo off
setlocal
title Johnny Castaway ESP32 Flasher
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Flash-JohnnyEsp32.ps1"
if errorlevel 1 (
  echo.
  echo Flashing did not complete. Read FLASHING.md for recovery steps.
  pause
  exit /b 1
)
echo.
pause
