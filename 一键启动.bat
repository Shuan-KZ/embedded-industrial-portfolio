@echo off
chcp 65001 >nul
title 设备监控系统 — 一键启动

echo ========================================
echo   设备监控系统 一键启动
echo   Simulation Mode - No PLC Hardware
echo ========================================
echo.

:: 1. 启动后端
echo [1/3] 启动后端 (SpringBoot + Flask)...
cd /d "%~dp0"
start "Backend" /MIN cmd /c "python launcher.py start"

:: 2. 等待后端就绪
echo [2/3] 等待后端就绪 (~30s)...
ping 127.0.0.1 -n 15 >nul

:: 3. 启动 HMI 上位机
echo [3/3] 启动 HMI 上位机...
if exist "%~dp002_上位机\DeviceHMI" (
    cd /d "%~dp002_上位机\DeviceHMI"
    start "HMI" cmd /c dotnet run
    cd /d "%~dp0"
) else (
    echo   HMI 目录未找到，跳过
)

:: 4. 打开浏览器大屏
ping 127.0.0.1 -n 5 >nul
start http://localhost:8081/dashboard.html

echo.
echo ========================================
echo   启动完成
echo   大屏: http://localhost:8081/dashboard.html
echo   Swagger: http://localhost:8081/swagger-ui/index.html
echo ========================================
echo.
echo 关闭此窗口不会停止服务
pause
