@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM  EDIT THIS SECTION ONLY
REM ============================================================

REM Optional override. Leave empty to auto-detect a local InjectorCLI.exe.
set "INJECTOR="

REM Local payload folder (next to this script). Subfolders are included.
set "PAYLOAD=%~dp0Payload"

REM Seconds to wait AFTER the game is found and BEFORE injecting (0 for none).
set "INJECT_DELAY=30"

REM --- Target 1 ---
set "PROC1=target.exe"
set "DLL1=payload.dll"
set "MAP1=STANDART"

REM --- Target 2 ---
set "PROC2=other.exe"
set "DLL2=other.dll"
set "MAP2=STANDART"

REM --- Target 3 (uncomment to enable) ---
REM set "PROC3=app.exe"
REM set "DLL3=app.dll"
REM set "MAP3=STANDART"

REM DLL1 / DLL2 / DLL3 are relative to the Payload folder (not a drive path).
REM Subfolders work, e.g.  tools\my.dll
REM MAP values: STANDART (LoadLibrary) or MANUAL (manual map)
REM ============================================================
REM  DO NOT EDIT BELOW
REM ============================================================

if "%INJECTOR%"=="" call :FindInjector
if not exist "%INJECTOR%" (
    echo [ERROR] InjectorCLI.exe was not found next to this script.
    echo Visual Studio builds it to: "%~dp0build\x64\Release\InjectorCLI.exe"
    echo Open Down'sInjector.sln, build InjectorCLI ^(x64 Release^), then run this again.
    pause
    exit /b 1
)
echo Using injector: "%INJECTOR%"

if not exist "%PAYLOAD%\" (
    mkdir "%PAYLOAD%" >nul 2>&1
    echo [ERROR] Payload folder was missing. Created: "%PAYLOAD%"
    echo Put your DLLs there, set DLL1 / DLL2 / DLL3 above, then run this again.
    pause
    exit /b 1
)

call :ResolvePayload "%DLL1%" FULL1
if errorlevel 1 exit /b 1
call :ResolvePayload "%DLL2%" FULL2
if errorlevel 1 exit /b 1
REM call :ResolvePayload "%DLL3%" FULL3
REM if errorlevel 1 exit /b 1

echo Starting monitor for "%PROC1%"  ->  "%DLL1%"
start "Injector-1" /b "%INJECTOR%" -m %MAP1% --auto_inject_process "%PROC1%" --inject_delay %INJECT_DELAY% "%FULL1%"

echo Starting monitor for "%PROC2%"  ->  "%DLL2%"
start "Injector-2" /b "%INJECTOR%" -m %MAP2% --auto_inject_process "%PROC2%" --inject_delay %INJECT_DELAY% "%FULL2%"

REM echo Starting monitor for "%PROC3%"  ->  "%DLL3%"
REM start "Injector-3" /b "%INJECTOR%" -m %MAP3% --auto_inject_process "%PROC3%" --inject_delay %INJECT_DELAY% "%FULL3%"

echo.
echo Both monitors running. Close this window to stop them.
echo.
:KeepRunning
timeout /t 86400 /nobreak >nul
goto KeepRunning

:ResolvePayload
set "REL=%~1"
set "OUTVAR=%~2"
if "%REL%"=="" (
    echo [ERROR] DLL path is empty. Set DLL1 / DLL2 / DLL3 to a file under Payload\.
    pause
    exit /b 1
)

echo %REL%| findstr /r "^[A-Za-z]:" >nul
if not errorlevel 1 (
    echo [ERROR] Use a path relative to Payload\, not a drive path: "%REL%"
    pause
    exit /b 1
)

set "CANDIDATE=%PAYLOAD%\%REL%"
if exist "%CANDIDATE%" (
    for %%F in ("%CANDIDATE%") do set "%OUTVAR%=%%~fF"
    exit /b 0
)

REM Exact relative path missed; search Payload recursively by file name.
set "WANTED=%~nx1"
set "FOUND="
for /r "%PAYLOAD%" %%F in (*.dll) do (
    if /I "%%~nxF"=="!WANTED!" (
        set "FOUND=%%~fF"
        goto :ResolveFound
    )
)

:ResolveFound
if not defined FOUND (
    echo [ERROR] Payload not found: "%REL%"
    echo Looked in: "%PAYLOAD%"
    echo Use a path relative to Payload\, e.g. my.dll or folder\my.dll
    pause
    exit /b 1
)
set "%OUTVAR%=!FOUND!"
exit /b 0

:FindInjector
for %%P in (
    "%~dp0InjectorCLI.exe"
    "%~dp0build\x64\Release\InjectorCLI.exe"
    "%~dp0build\x64\Debug\InjectorCLI.exe"
    "%~dp0InjectorCLI\x64\Release\InjectorCLI.exe"
    "%~dp0InjectorCLI\x64\Debug\InjectorCLI.exe"
    "%~dp0x64\Release\InjectorCLI.exe"
    "%~dp0x64\Debug\InjectorCLI.exe"
) do (
    if exist %%P (
        set "INJECTOR=%%~fP"
        exit /b 0
    )
)
for /r "%~dp0" %%F in (InjectorCLI.exe) do (
    set "CAND=%%~fF"
    echo !CAND!| findstr /i "\\Payload\\ \\Blackbone\\ \\vcpkg" >nul
    if errorlevel 1 (
        set "INJECTOR=%%~fF"
        exit /b 0
    )
)
exit /b 1
