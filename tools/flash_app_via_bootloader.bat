@echo off
setlocal

set "ROOT=%~dp0.."
set "OBJCOPY_EXE=%OBJCOPY_EXE%"
if "%OBJCOPY_EXE%"=="" set "OBJCOPY_EXE=C:\Users\kac\.niiet_aspect\riscv_gcc_windows\bin\riscv64-unknown-elf-objcopy.exe"
set "PYTHON_EXE=%PYTHON_EXE%"
if "%PYTHON_EXE%"=="" set "PYTHON_EXE=python"

set "APP_HEX=%APP_HEX%"
if "%APP_HEX%"=="" set "APP_HEX=%ROOT%\vg015\build\vg015.hex"
set "APP_BIN=%APP_BIN%"
if "%APP_BIN%"=="" set "APP_BIN=%ROOT%\vg015\build\vg015.bin"
set "BOOT_PORT=%BOOT_PORT%"
if "%BOOT_PORT%"=="" set "BOOT_PORT=auto"
set "BOOT_BAUD=%BOOT_BAUD%"
if "%BOOT_BAUD%"=="" set "BOOT_BAUD=115200"
set "BOOT_TIMEOUT=%BOOT_TIMEOUT%"
if "%BOOT_TIMEOUT%"=="" set "BOOT_TIMEOUT=30"

if "%DRY_RUN%"=="1" (
  echo DRY RUN: call "%ROOT%\tools\build_app.bat"
  echo DRY RUN: "%OBJCOPY_EXE%" -I ihex -O binary "%APP_HEX%" "%APP_BIN%"
  echo DRY RUN: "%PYTHON_EXE%" "%ROOT%\bootloader\boot.py" --port "%BOOT_PORT%" --bin "%APP_BIN%" --baud "%BOOT_BAUD%" --timeout "%BOOT_TIMEOUT%"
  exit /b 0
)

call "%ROOT%\tools\build_app.bat"
if errorlevel 1 exit /b %ERRORLEVEL%

"%OBJCOPY_EXE%" -I ihex -O binary "%APP_HEX%" "%APP_BIN%"
if errorlevel 1 exit /b %ERRORLEVEL%

"%PYTHON_EXE%" "%ROOT%\bootloader\boot.py" --port "%BOOT_PORT%" --bin "%APP_BIN%" --baud "%BOOT_BAUD%" --timeout "%BOOT_TIMEOUT%"
exit /b %ERRORLEVEL%
