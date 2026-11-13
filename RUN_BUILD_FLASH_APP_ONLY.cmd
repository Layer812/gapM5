@REM Copyright (c) 2026 LAYER8. SPDX-License-Identifier: MIT
@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "PORT=%~1"
if "%PORT%"=="" (echo Usage: RUN_BUILD_FLASH_APP_ONLY.cmd COMxx & exit /b 2)
call "%~dp0RUN_BUILD_ONLY.cmd"
if errorlevel 1 (echo STOP: native build failed; no flash. & exit /b 3)
if not exist "%~dp0.pio\build\app_native\firmware.bin" (echo STOP: missing firmware; no flash. & exit /b 4)
python "%~dp0flash_app_only.py" --port "%PORT%" --bin "%~dp0.pio\build\app_native\firmware.bin"
if errorlevel 1 (echo STOP: APP-only write aborted. & exit /b 5)
echo PASS: GapM5 APP-only written. If in ROM mode, release boot keys and power-cycle.
exit /b 0
