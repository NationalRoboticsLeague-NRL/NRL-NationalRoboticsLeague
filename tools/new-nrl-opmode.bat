@echo off
rem ===================================================================
rem  NRL - Create a New OpMode  (double-click launcher, Windows)
rem  Scaffolds a robot OpMode from a minimal template, then opens it.
rem ===================================================================
setlocal
title NRL - Create a New OpMode

rem This .bat lives in <repo>\tools\ ; the generator sits next to it.
set "SCRIPT=%~dp0nrl_new_opmode.py"

rem Find a usable Python 3 (real py/python); if none is found, ensure-python.ps1
rem installs one automatically and prints its path.
set "PYEXE="
for /f "usebackq delims=" %%i in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ensure-python.ps1"`) do set "PYEXE=%%i"
if not defined PYEXE goto no_python
"%PYEXE%" "%SCRIPT%" --open %*
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
