@echo off
setlocal

set "ROOT=%~dp0.."
set "OPENOCD_EXE=%OPENOCD_EXE%"
if "%OPENOCD_EXE%"=="" set "OPENOCD_EXE=C:\Users\kac\.niiet_aspect\riscv_kit_windows\bin\openocd.exe"

set "OPENOCD_SCRIPTS_ROOT=%OPENOCD_SCRIPTS_ROOT%"
if "%OPENOCD_SCRIPTS_ROOT%"=="" set "OPENOCD_SCRIPTS_ROOT=C:\Users\kac\.niiet_aspect\riscv_kit_windows"

set "OPENOCD_INTERFACE_CFG=%OPENOCD_INTERFACE_CFG%"
if "%OPENOCD_INTERFACE_CFG%"=="" set "OPENOCD_INTERFACE_CFG=./interface/onboard_ftdi.cfg"

set "OPENOCD_TARGET_CFG=%OPENOCD_TARGET_CFG%"
if "%OPENOCD_TARGET_CFG%"=="" set "OPENOCD_TARGET_CFG=./target/k1921vg015.cfg"

set "OPENOCD_INIT_CMD=%OPENOCD_INIT_CMD%"
if "%OPENOCD_INIT_CMD%"=="" set "OPENOCD_INIT_CMD=init;halt"

if "%DRY_RUN%"=="1" (
  echo DRY RUN: "%OPENOCD_EXE%" -s "%OPENOCD_SCRIPTS_ROOT%" -s "%OPENOCD_SCRIPTS_ROOT%\interface" -s "%OPENOCD_SCRIPTS_ROOT%\target" -f "%OPENOCD_INTERFACE_CFG%" -f "%OPENOCD_TARGET_CFG%" -c "%OPENOCD_INIT_CMD%" -d1
  exit /b 0
)

pushd "%ROOT%\vg015" || exit /b 1
"%OPENOCD_EXE%" -s "%OPENOCD_SCRIPTS_ROOT%" -s "%OPENOCD_SCRIPTS_ROOT%\interface" -s "%OPENOCD_SCRIPTS_ROOT%\target" -f "%OPENOCD_INTERFACE_CFG%" -f "%OPENOCD_TARGET_CFG%" -c "%OPENOCD_INIT_CMD%" -d1
set "RC=%ERRORLEVEL%"
popd
exit /b %RC%
