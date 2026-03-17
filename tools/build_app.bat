@echo off
setlocal

set "ROOT=%~dp0.."
set "MAKE_EXE=%MAKE_EXE%"
if "%MAKE_EXE%"=="" set "MAKE_EXE=C:\Users\kac\.niiet_aspect\riscv_kit_windows\bin\make.exe"

set "COMPILER_PATH=%COMPILER_PATH%"
if "%COMPILER_PATH%"=="" set "COMPILER_PATH=C:/Users/kac/.niiet_aspect/riscv_gcc_windows/bin"

set "PREFIX=%PREFIX%"
if "%PREFIX%"=="" set "PREFIX=riscv64-unknown-elf-"

set "MARCH=%MARCH%"
if "%MARCH%"=="" set "MARCH=rv32imfc_zicsr"

set "MABI=%MABI%"
if "%MABI%"=="" set "MABI=ilp32f"

set "OPTIMISATION=%OPTIMISATION%"
if "%OPTIMISATION%"=="" set "OPTIMISATION=-O0"

pushd "%ROOT%\vg015" || exit /b 1
"%MAKE_EXE%" all COMPILER_PATH=%COMPILER_PATH% PREFIX=%PREFIX% MARCH=%MARCH% MABI=%MABI% OPTIMISATION=%OPTIMISATION%
set "RC=%ERRORLEVEL%"
popd
exit /b %RC%
