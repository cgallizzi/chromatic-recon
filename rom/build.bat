@echo off
REM POCKET RECON - Windows build (no `make` needed)
REM 1. Download GBDK-2020:  https://github.com/gbdk-2020/gbdk-2020/releases
REM 2. Unzip it, then set GBDK below to that folder (the one containing bin\).

set GBDK=C:\gbdk

if not exist "%GBDK%\bin\lcc.exe" (
  echo.
  echo   ERROR: lcc.exe not found at "%GBDK%\bin\lcc.exe"
  echo   Edit build.bat and set GBDK to your GBDK-2020 folder.
  echo.
  exit /b 1
)

echo Building pocketrecon.gb ...
"%GBDK%\bin\lcc" -Wa-l -Wl-j -Wm-yC -o pocketrecon.gb main.c scan_mock.c
if errorlevel 1 (
  echo BUILD FAILED.
  exit /b 1
)
echo.
echo   OK -^> pocketrecon.gb
echo   Test it in an emulator (Emulicious / BGB / SameBoy),
echo   then load it on a GB flashcart to run on the Chromatic.
