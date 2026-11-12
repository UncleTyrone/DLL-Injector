@echo off
setlocal
cd /d "%~dp0"

net session >nul 2>&1
if errorlevel 1 (
    echo Requesting Administrator permission...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -WorkingDirectory '%~dp0' -Verb RunAs"
    exit /b
)

set "INJECTOR="
if exist "%~dp0InjectorCLI.exe" set "INJECTOR=%~dp0InjectorCLI.exe"
if "%INJECTOR%"=="" if exist "%~dp0build\x64\Release\InjectorCLI.exe" set "INJECTOR=%~dp0build\x64\Release\InjectorCLI.exe"
if "%INJECTOR%"=="" if exist "%~dp0build\x64\Debug\InjectorCLI.exe" set "INJECTOR=%~dp0build\x64\Debug\InjectorCLI.exe"
if "%INJECTOR%"=="" (
    echo [ERROR] InjectorCLI.exe not found next to this script.
    pause
    exit /b 1
)

"%INJECTOR%" --service uninstall --config "%~dp0inject-service.ini"
pause
