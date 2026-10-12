@echo off
REM Chromatic Recon - PCAP capture helper (Windows)
REM Usage:  capture_pcap.bat [PORT] [OUTFILE]
REM   double-click for defaults (COM10, timestamped file in this folder)
REM   or:  capture_pcap.bat COM10 mycapture.pcap
setlocal

set "PORT=%~1"
if "%PORT%"=="" set "PORT=COM10"

for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "TS=%%i"
set "OUT=%~2"
if "%OUT%"=="" set "OUT=%~dp0capture_%TS%.pcap"

where python >nul 2>&1
if errorlevel 1 (
  echo.
  echo   Python 3 was not found on PATH. Install it from python.org and retry.
  echo.
  pause
  exit /b 1
)

python -c "import serial" >nul 2>&1
if errorlevel 1 (
  echo Installing pyserial...
  python -m pip install pyserial
)

echo.
echo   Capturing from %PORT%  -^>  %OUT%
echo   On the Chromatic: open the PCAP tab, pick a channel, press A to start.
echo   Press Ctrl-C here to stop.
echo.

python "%~dp0pcap_receiver.py" %PORT% "%OUT%"

echo.
echo   Saved: %OUT%
echo   Open it in Wireshark.
echo.
pause
