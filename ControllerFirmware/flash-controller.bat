@echo off
rem ===================================================================
rem  NRL - Flash the Controller  (double-click launcher, Windows)
rem  Writes the prebuilt controller firmware to a plugged-in controller.
rem ===================================================================
setlocal
title NRL - Flash Controller

set "SCRIPT=%~dp0flash_controller.py"

rem Find a usable Python 3 via the kit's installer helper (in ..\tools\);
rem if this ControllerFirmware folder was copied without tools\, fall back
rem to whatever py/python is already on this PC.
set "ENSURE=%~dp0..\tools\ensure-python.ps1"
set "PYEXE="
if exist "%ENSURE%" (
    for /f "usebackq delims=" %%i in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%ENSURE%"`) do set "PYEXE=%%i"
)
if defined PYEXE goto run

where py >nul 2>nul
if errorlevel 1 goto try_python
set "PYEXE=py"
goto run

:try_python
where python >nul 2>nul
if errorlevel 1 goto no_python
set "PYEXE=python"
goto run

:run
"%PYEXE%" "%SCRIPT%" %*
goto end

:no_python
echo.
echo [error] Python 3 was not found and could not be installed automatically.
echo   Install Python 3 from https://www.python.org/downloads/ and re-run,
echo   ticking "Add python.exe to PATH" during setup.

:end
echo.
if not defined NRL_NO_PAUSE pause
endlocal
