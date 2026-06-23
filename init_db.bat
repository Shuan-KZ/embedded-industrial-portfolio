@echo off
chcp 65001 >nul
echo ========================================
echo   设备监控系统 - 数据库初始化
echo ========================================
echo.
echo 正在创建数据库和表结构...

mysql -u root -p123456 < "03_后端\sql\init.sql"

if %errorlevel% equ 0 (
    echo.
    echo [OK] 数据库初始化完成
) else (
    echo.
    echo [FAIL] 初始化失败，请检查：
    echo   1. MySQL 是否已安装并启动
    echo   2. root 密码是否为 123456
    echo   3. 3306 端口是否被占用
)
echo.
pause
