@echo off
rem ===================================================================
rem  HexaSDK - Create a New Project  (double-click launcher, Windows)
rem  Runs the wizard, then opens the generated project in VS Code.
rem  You do NOT need to open the template in VS Code first.
rem ===================================================================
setlocal
title HexaSDK - Create a New Project

rem This .bat lives in <repo>\tools\ ; the generator sits next to it.
set "SCRIPT=%~dp0nrl_new_project.py"

rem Find a usable Python 3 (real py/python with tkinter); if none is found,
rem ensure-python.ps1 installs one automatically and prints its path.
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
