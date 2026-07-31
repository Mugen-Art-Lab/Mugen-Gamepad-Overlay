@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch-existing-project.ps1"
set "ERR=%ERRORLEVEL%"
echo.
if not "%ERR%"=="0" echo Patch/build failed with code %ERR%.
pause
exit /b %ERR%
