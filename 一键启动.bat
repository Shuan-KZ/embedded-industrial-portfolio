@echo off
chcp 65001 >nul
title Industrial Monitor - One Click Start
cd /d "%~dp0"

echo ========================================
echo   Industrial Monitor Quick Start
echo   Simulation Mode - No PLC Hardware
echo ========================================
echo.

python launcher.py start

pause
