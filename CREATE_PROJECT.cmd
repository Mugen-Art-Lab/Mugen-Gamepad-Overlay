@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\setup-project.ps1"
if errorlevel 1 (
  echo.
  echo Project creation failed.
  pause
  exit /b 1
)
echo.
echo The full project is in the Mugen-Gamepad-Overlay folder.
pause
