@echo off
chcp 65001 >nul
echo ========================================
echo   Device Monitor - DB Init
echo ========================================
echo.
echo Creating database and tables...

mysql -u root -p123456 < "03_后端\sql\init.sql"

if %errorlevel% equ 0 (
    echo.
    echo [OK] Database initialized
) else (
    echo.
    echo [FAIL] Init failed. Check:
    echo   1. MySQL installed and running
    echo   2. root password = 123456
    echo   3. Port 3306 available
)
echo.
pause
