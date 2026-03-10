@echo off
setlocal

set "ROOT=%~dp0.."
set "GDB_EXE=%GDB_EXE%"
if "%GDB_EXE%"=="" set "GDB_EXE=C:\Users\kac\.niiet_aspect\riscv_gcc_windows\bin\riscv64-unknown-elf-gdb.exe"
set "BOOTLOADER_ELF=%BOOTLOADER_ELF%"
if "%BOOTLOADER_ELF%"=="" set "BOOTLOADER_ELF=%ROOT%\bootloader\build\bootloader.elf"

if "%DRY_RUN%"=="1" (
  echo DRY RUN: call "%ROOT%\tools\build_bootloader.bat"
  echo DRY RUN: "%GDB_EXE%" "%BOOTLOADER_ELF%" -q -batch -ex="set confirm off" -ex="target extended-remote localhost:3333" -ex="monitor reset halt" -ex="load" -ex="monitor reset run" -ex="disconnect" -ex="quit"
  exit /b 0
)

call "%ROOT%\tools\build_bootloader.bat"
if errorlevel 1 exit /b %ERRORLEVEL%

"%GDB_EXE%" "%BOOTLOADER_ELF%" -q -batch -ex="set confirm off" -ex="target extended-remote localhost:3333" -ex="monitor reset halt" -ex="load" -ex="monitor reset run" -ex="disconnect" -ex="quit"
exit /b %ERRORLEVEL%
