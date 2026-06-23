@echo off
chcp 65001 >nul
title Industrial Monitor - One Click Start

echo ========================================
echo   Industrial Monitor Quick Start
echo   Simulation Mode - No PLC Hardware
echo ========================================
echo.

echo [1/3] Starting backend (SpringBoot + Flask)...
cd /d "%~dp0"
start "Backend" /MIN cmd /c "python launcher.py start"

echo [2/3] Waiting for backend (~30s)...
ping 127.0.0.1 -n 15 >nul

echo [3/3] Starting HMI...
if exist "%~dp002_上位机\DeviceHMI" (
    cd /d "%~dp002_上位机\DeviceHMI"
    start "HMI" cmd /c dotnet run
    cd /d "%~dp0"
) else (
    echo   HMI folder not found - skipping
)

ping 127.0.0.1 -n 5 >nul
start http://localhost:8081/dashboard.html

echo.
echo ========================================
echo   System Ready
echo   Dashboard:  http://localhost:8081/dashboard.html
echo   Swagger:    http://localhost:8081/swagger-ui/index.html
echo ========================================
echo.
echo Close this window - services keep running
pause
