@echo off
title Water Curtain Backend
cd /d "%~dp0"

echo ==========================================
echo   Water Curtain Controller - Backend
echo ==========================================

:: Check Python
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python khong duoc tim thay. Hay cai Python 3.8+
    pause
    exit /b 1
)

:: Install/check dependencies
echo [*] Kiem tra dependencies...
pip install -r requirements_backend.txt -q

echo [*] Khoi dong backend...
echo [*] Nhan Ctrl+C de dung
echo.
python backend.py

pause
