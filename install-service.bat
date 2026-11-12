@echo off
setlocal EnableDelayedExpansion
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
    echo [ERROR] InjectorCLI.exe not found next to this script, and no Release build is present.
    pause
    exit /b 1
)

if not exist "%~dp0inject-service.ini" (
    if exist "%~dp0inject-service.ini.example" (
        copy /Y "%~dp0inject-service.ini.example" "%~dp0inject-service.ini" >nul
        echo Created inject-service.ini from the example. Edit it, then run this again.
        notepad "%~dp0inject-service.ini"
        pause
        exit /b 1
    )
    echo [ERROR] Missing inject-service.ini next to this script.
    pause
    exit /b 1
)

echo Installing Windows service using:
echo   "%INJECTOR%"
echo Config: "%~dp0inject-service.ini"
echo Edit that ini for processes, DLLs, mapping, and delay, then restart the service.
echo.
"%INJECTOR%" --service install --config "%~dp0inject-service.ini"
if errorlevel 1 (
    pause
    exit /b 1
)

echo.
echo Logs: inject-service.log next to inject-service.ini
echo Stop / remove with uninstall-service.bat
pause
