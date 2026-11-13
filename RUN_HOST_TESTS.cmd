@REM Copyright (c) 2026 LAYER8. SPDX-License-Identifier: MIT
@echo off
setlocal EnableExtensions
cd /d "%~dp0"
where g++ >nul 2>nul
if errorlevel 1 (echo ERROR: g++ is required for optional host logic test. & exit /b 1)
g++ -std=c++17 -O2 tests\test_core.cpp src\game_core.cpp src\net_protocol.cpp -o tests\test_core.exe
if errorlevel 1 exit /b 2
tests\test_core.exe
exit /b %errorlevel%
