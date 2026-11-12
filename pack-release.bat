@echo off
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
set "STAGE=%ROOT%dist\InjectorCLI"
set "ZIP=%ROOT%dist\InjectorCLI-Release.zip"
set "EXE=%ROOT%build\x64\Release\InjectorCLI.exe"

if not exist "%EXE%" (
    echo [ERROR] Build InjectorCLI Release x64 first: "%EXE%"
    exit /b 1
)

echo Staging portable release in "%STAGE%"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\Payload" >nul

copy /Y "%EXE%" "%STAGE%\InjectorCLI.exe" >nul
copy /Y "%ROOT%build\x64\Release\msdia140.dll" "%STAGE%\" >nul
copy /Y "%ROOT%build\x64\Release\symsrv.dll" "%STAGE%\" >nul
copy /Y "%ROOT%inject-launcher.bat" "%STAGE%\" >nul
copy /Y "%ROOT%inject-launcher-hidden.vbs" "%STAGE%\" >nul
copy /Y "%ROOT%install-service.bat" "%STAGE%\" >nul
copy /Y "%ROOT%uninstall-service.bat" "%STAGE%\" >nul
copy /Y "%ROOT%inject-service.ini.example" "%STAGE%\" >nul
copy /Y "%ROOT%inject-service.ini.example" "%STAGE%\inject-service.ini" >nul
if exist "%ROOT%Payload\README.md" copy /Y "%ROOT%Payload\README.md" "%STAGE%\Payload\" >nul
copy /Y "%ROOT%Payload\.gitkeep" "%STAGE%\Payload\" >nul 2>nul

(
echo InjectorCLI portable release
echo ============================
echo.
echo 64-bit Windows only. Unzip to a folder and keep all files together.
echo That folder IS the install. Do not delete it after installing the service.
echo.
echo Console:
echo   1. Put DLLs in Payload\
echo   2. Edit inject-launcher.bat ^(PROC / DLL / MAP / INJECT_DELAY^)
echo   3. Double-click inject-launcher.bat
echo.
echo Windows service:
echo   1. Copy inject-service.ini.example to inject-service.ini if needed
echo   2. Edit inject-service.ini
echo   3. Double-click install-service.bat and approve UAC
echo   4. After ini changes: net stop InjectorCLI ^& net start InjectorCLI
echo   5. Double-click uninstall-service.bat to remove the service
echo.
echo Service log: inject-service.log in this folder.
) > "%STAGE%\README.txt"

if exist "%ZIP%" del /f /q "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%ZIP%' -Force"
if errorlevel 1 (
    echo [ERROR] Failed to create zip.
    exit /b 1
)

echo.
echo Release folder: "%STAGE%"
echo Zip:            "%ZIP%"
dir "%ZIP%" | findstr /i "InjectorCLI-Release.zip"
