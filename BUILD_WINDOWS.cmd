@echo off
setlocal
set "ROOT=%~dp0"
set "WORKSPACE=%ROOT%build-workspace"

rem Create the ignored generated OBS workspace on first use.
if not exist "%WORKSPACE%\cmake\common\bootstrap.cmake" (
  echo Creating the local OBS plugin build workspace...
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\setup-project.ps1" -Destination "%WORKSPACE%"
  if errorlevel 1 (
    echo.
    echo Workspace creation failed.
    pause
    exit /b 1
  )
)

rem Keep a single public launcher in the repository root. The generated
rem workspace contains only the PowerShell worker used for the actual build.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%WORKSPACE%\tools\build-windows.ps1" -Configuration Release
set "BUILD_EXIT=%ERRORLEVEL%"
if not "%BUILD_EXIT%"=="0" (
  echo.
  echo Build failed.
)
pause
exit /b %BUILD_EXIT%
