@echo off
setlocal
set "ROOT=%~dp0"

rem In the compact package, delegate to the generated project if it exists.
if not exist "%ROOT%cmake\common\bootstrap.cmake" (
  if exist "%ROOT%Mugen-Gamepad-Overlay\BUILD_WINDOWS.cmd" (
    echo Full project found. Starting its build...
    call "%ROOT%Mugen-Gamepad-Overlay\BUILD_WINDOWS.cmd"
    exit /b %errorlevel%
  )

  echo.
  echo Run CREATE_PROJECT.cmd first.
  echo After it creates Mugen-Gamepad-Overlay, run this same BUILD_WINDOWS.cmd again.
  pause
  exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\build-windows.ps1" -Configuration RelWithDebInfo
if errorlevel 1 (
  echo.
  echo Build failed.
  pause
  exit /b 1
)
pause
