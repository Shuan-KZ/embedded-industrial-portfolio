@echo off
chcp 65001 >nul
cd /d "%~dp0"
setlocal enabledelayedexpansion

echo ========================================
echo   Industrial Monitor - Quick Start
echo ========================================
echo.

echo [Check] Environment...
where java >nul 2>&1
if %errorlevel% neq 0 (
    echo [FAIL] Java not found. Install JDK 17+
    pause & exit /b 1
)
where python >nul 2>&1
if %errorlevel% neq 0 (
    echo [FAIL] Python not found. Install Python 3.10+
    pause & exit /b 1
)
where dotnet >nul 2>&1
if %errorlevel% neq 0 (
    echo [WARN] .NET SDK not found. HMI unavailable.
)
echo [OK] H2 embedded DB mode - no MySQL needed
echo.

echo [Start] SpringBoot (:8081) ...
start "SpringBoot" /MIN cmd /c "cd /d "%~dp0monitor_server\demo" && mvnw.cmd spring-boot:run -q"

echo [Start] Flask IMOGJO (:5000) ...
start "Flask" /MIN cmd /c "cd /d "%~dp005_算法服务" && python scheduler.py"

echo [Wait] Starting services...
set /a count=0
:wait_loop
timeout /t 2 /nobreak >nul
set /a count+=2
curl -s http://localhost:8081/api/devices >nul 2>&1
if %errorlevel% equ 0 goto ready
if %count% lss 60 goto wait_loop
echo [WARN] Timeout - check services manually
goto open

:ready
echo [OK] Services ready (%count%s)
echo.

:open
echo [Open] Dashboard...
start http://localhost:8081/dashboard.html

echo.
echo ========================================
echo   System Ready
echo   Dashboard:  http://localhost:8081/dashboard.html
echo   Swagger:    http://localhost:8081/swagger-ui/index.html
echo ========================================
echo.
echo Close this window to stop services
pause >nul
