@echo off
chcp 65001 >nul
cd /d "%~dp0"
setlocal enabledelayedexpansion

echo ========================================
echo   设备监控系统 - 一键启动
echo ========================================
echo.

REM --- 环境检测 ---
echo [检测] 环境检查...

where java >nul 2>&1
if %errorlevel% neq 0 (
    echo [FAIL] 未找到 Java，请安装 JDK 17+
    pause & exit /b 1
)

where python >nul 2>&1
if %errorlevel% neq 0 (
    echo [FAIL] 未找到 Python，请安装 Python 3.10+
    pause & exit /b 1
)

where dotnet >nul 2>&1
if %errorlevel% neq 0 (
    echo [WARN] 未找到 .NET SDK，HMI 上位机将无法启动
)

echo [OK] 环境检测通过（默认 H2 内嵌数据库，无需 MySQL）
echo.

REM --- 启动后端 ---
echo [启动] SpringBoot (:8081) ...
REM  默认 H2 内嵌数据库。如需切 MySQL：把下面一行注释掉，取消下一行的注释
start "SpringBoot" /MIN cmd /c "cd /d "%~dp0monitor_server\demo" && mvnw.cmd spring-boot:run -q"
REM start "SpringBoot" /MIN cmd /c "cd /d "%~dp0monitor_server\demo" && mvnw spring-boot:run -q -Dspring-boot.run.profiles=mysql"

echo [启动] Flask (:5000) ...
start "Flask" /MIN cmd /c "cd /d "%~dp005_算法服务" && python scheduler.py"

REM --- 等待服务就绪 ---
echo [等待] 服务启动中...
set /a count=0
:wait_loop
timeout /t 2 /nobreak >nul
set /a count+=2
curl -s http://localhost:8081/api/devices >nul 2>&1
if %errorlevel% equ 0 goto ready
if %count% lss 60 goto wait_loop
echo [WARN] 服务启动超时，请手动检查
goto open

:ready
echo [OK] 服务就绪 (%count%s)
echo.

REM --- 打开大屏 ---
:open
echo [打开] 浏览器大屏...
start http://localhost:8081/dashboard.html

echo.
echo ========================================
echo   启动完成
echo   大屏: http://localhost:8081/dashboard.html
echo   Swagger: http://localhost:8081/swagger-ui/index.html
echo ========================================
echo.
echo 按任意键关闭此窗口（不会停止服务）
pause >nul
