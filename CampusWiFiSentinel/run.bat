@echo off
REM One-click: build the latest server and start it.
cd /d "%~dp0"

call build.bat
if errorlevel 1 (
    echo.
    echo Build failed - see the messages above.
    pause
    exit /b 1
)

echo.
echo Starting Campus WiFi Sentinel...
echo Dashboard:  http://localhost:8080
echo Stop the server with Ctrl+C.
echo.
start "" http://localhost:8080
bin\campus_wifi_server.exe
pause
