@echo off
setlocal

rem Keep user-provided values as highest priority.
if not "%MAKE_EXE%"=="" goto :after_make
if exist "C:\Users\kac\.niiet_aspect\riscv_kit_windows\bin\make.exe" set "MAKE_EXE=C:\Users\kac\.niiet_aspect\riscv_kit_windows\bin\make.exe"
:after_make

if not "%GDB_EXE%"=="" goto :after_gdb
if exist "C:\Users\kac\.niiet_aspect\riscv_gcc_windows\bin\riscv64-unknown-elf-gdb.exe" set "GDB_EXE=C:\Users\kac\.niiet_aspect\riscv_gcc_windows\bin\riscv64-unknown-elf-gdb.exe"
:after_gdb

if not "%OBJCOPY_EXE%"=="" goto :after_objcopy
if exist "C:\Users\kac\.niiet_aspect\riscv_gcc_windows\bin\riscv64-unknown-elf-objcopy.exe" set "OBJCOPY_EXE=C:\Users\kac\.niiet_aspect\riscv_gcc_windows\bin\riscv64-unknown-elf-objcopy.exe"
:after_objcopy

if not "%OPENOCD_EXE%"=="" goto :after_openocd
if exist "C:\Users\kac\.niiet_aspect\riscv_kit_windows\bin\openocd.exe" set "OPENOCD_EXE=C:\Users\kac\.niiet_aspect\riscv_kit_windows\bin\openocd.exe"
:after_openocd

if "%COMPILER_PATH%"=="" (
  if exist "C:\Users\kac\.niiet_aspect\riscv_gcc_windows\bin\riscv64-unknown-elf-gcc.exe" (
    set "COMPILER_PATH=C:/Users/kac/.niiet_aspect/riscv_gcc_windows/bin"
  )
)

if "%OPENOCD_SCRIPTS_ROOT%"=="" (
  if exist "C:\Users\kac\.niiet_aspect\riscv_kit_windows\target" (
    set "OPENOCD_SCRIPTS_ROOT=C:\Users\kac\.niiet_aspect\riscv_kit_windows"
  ) else if not "%OPENOCD_EXE%"=="" (
    for %%I in ("%OPENOCD_EXE%") do set "OPENOCD_SCRIPTS_ROOT=%%~dpI.."
  )
)

if "%PYTHON_EXE%"=="" set "PYTHON_EXE=python"

endlocal & (
  if not "%MAKE_EXE%"=="" set "MAKE_EXE=%MAKE_EXE%"
  if not "%GDB_EXE%"=="" set "GDB_EXE=%GDB_EXE%"
  if not "%OBJCOPY_EXE%"=="" set "OBJCOPY_EXE=%OBJCOPY_EXE%"
  if not "%OPENOCD_EXE%"=="" set "OPENOCD_EXE=%OPENOCD_EXE%"
  if not "%OPENOCD_SCRIPTS_ROOT%"=="" set "OPENOCD_SCRIPTS_ROOT=%OPENOCD_SCRIPTS_ROOT%"
  if not "%COMPILER_PATH%"=="" set "COMPILER_PATH=%COMPILER_PATH%"
  if not "%PYTHON_EXE%"=="" set "PYTHON_EXE=%PYTHON_EXE%"
)
exit /b 0
