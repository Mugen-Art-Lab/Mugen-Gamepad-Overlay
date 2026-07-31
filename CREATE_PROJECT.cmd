@echo off
setlocal
set "ROOT=%~dp0"
set "WORKSPACE=%ROOT%build-workspace"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\setup-project.ps1" -Destination "%WORKSPACE%"
if errorlevel 1 (
  echo.
  echo Workspace creation failed.
  pause
  exit /b 1
)

echo.
echo The generated OBS plugin project is in:
echo %WORKSPACE%
echo.
echo Run BUILD_WINDOWS.cmd from the repository root to build and package it.
pause
