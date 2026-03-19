@echo off
setlocal

set "ROOT=%~dp0.."
call "%~dp0toolchain_env.bat"
if "%OBJCOPY_EXE%"=="" (
  echo ERROR: OBJCOPY_EXE not found. Set OBJCOPY_EXE or add riscv64-unknown-elf-objcopy.exe to PATH.
  exit /b 1
)

set "APP_HEX=%APP_HEX%"
if "%APP_HEX%"=="" set "APP_HEX=%ROOT%\vg015\build\vg015.hex"
set "APP_BIN=%APP_BIN%"
if "%APP_BIN%"=="" set "APP_BIN=%ROOT%\vg015\build\vg015.bin"

if "%DRY_RUN%"=="1" (
  echo DRY RUN: "%OBJCOPY_EXE%" -I ihex -O binary "%APP_HEX%" "%APP_BIN%"
  exit /b 0
)

"%OBJCOPY_EXE%" -I ihex -O binary "%APP_HEX%" "%APP_BIN%"
exit /b %ERRORLEVEL%
