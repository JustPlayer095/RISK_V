@echo off
setlocal

set "ROOT=%~dp0.."
set "GDB_EXE=%GDB_EXE%"
if "%GDB_EXE%"=="" set "GDB_EXE=C:\Users\kac\.niiet_aspect\riscv_gcc_windows\bin\riscv64-unknown-elf-gdb.exe"
set "APP_ELF=%APP_ELF%"
if "%APP_ELF%"=="" set "APP_ELF=%ROOT%\vg015\build\vg015.elf"

if "%DRY_RUN%"=="1" (
  echo DRY RUN: "%GDB_EXE%" "%APP_ELF%" -iex="target extended-remote localhost:3333" -ex="monitor halt"
  exit /b 0
)

"%GDB_EXE%" "%APP_ELF%" -iex="target extended-remote localhost:3333" -ex="monitor halt"
exit /b %ERRORLEVEL%
