@echo off
setlocal

set "ROOT=%~dp0.."
call "%~dp0toolchain_env.bat"

set "COMPILER_PATH=%COMPILER_PATH%"
if "%COMPILER_PATH%"=="" (
  echo ERROR: COMPILER_PATH not found. Set COMPILER_PATH or add riscv64-unknown-elf-gcc.exe to PATH.
  exit /b 1
)
if "%MAKE_EXE%"=="" (
  echo ERROR: MAKE_EXE not found. Set MAKE_EXE or add make.exe to PATH.
  exit /b 1
)

set "PREFIX=%PREFIX%"
if "%PREFIX%"=="" set "PREFIX=riscv64-unknown-elf-"

set "MARCH=%MARCH%"
if "%MARCH%"=="" set "MARCH=rv32imfc_zicsr"

set "MABI=%MABI%"
if "%MABI%"=="" set "MABI=ilp32f"

set "OPTIMISATION=%OPTIMISATION%"
if "%OPTIMISATION%"=="" set "OPTIMISATION=-O0"

pushd "%ROOT%\bootloader" || exit /b 1
"%MAKE_EXE%" all COMPILER_PATH=%COMPILER_PATH% PREFIX=%PREFIX% MARCH=%MARCH% MABI=%MABI% OPTIMISATION=%OPTIMISATION%
set "RC=%ERRORLEVEL%"
popd
exit /b %RC%
