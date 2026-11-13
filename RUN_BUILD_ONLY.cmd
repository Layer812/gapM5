@REM Copyright (c) 2026 LAYER8. SPDX-License-Identifier: MIT
@echo off
setlocal EnableExtensions
cd /d "%~dp0"
where pio >nul 2>nul
if errorlevel 1 (echo ERROR: PlatformIO CLI [pio] is missing. & exit /b 1)
pio run -e app_native
if errorlevel 1 (echo ERROR: Build failed. No flash performed. & exit /b 2)
echo PASS: GapM5 R7 native APP built. No flash performed.
exit /b 0
