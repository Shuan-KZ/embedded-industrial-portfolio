@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ========================================
echo   Industrial Monitor - One Click Start
echo ========================================
echo.
echo   Simulation Mode - No PLC Hardware Required
echo.
echo   Manual steps (do before start):
echo     [1] NetToPLCsim Run as Admin -^> Start Server
echo     [2] TIA Portal -^> PLCSIM -^> Download -^> RUN
echo.
echo ========================================
echo.

python launcher.py start

pause
