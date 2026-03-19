@echo off
setlocal EnableExtensions

if not defined NIIET_ASPECT_HOME (
  if exist "%USERPROFILE%\.niiet_aspect" set "NIIET_ASPECT_HOME=%USERPROFILE%\.niiet_aspect"
)
if not defined NIIET_ASPECT_HOME (
  if exist "%HOMEDRIVE%%HOMEPATH%\.niiet_aspect" set "NIIET_ASPECT_HOME=%HOMEDRIVE%%HOMEPATH%\.niiet_aspect"
)

if not defined MAKE_EXE if defined NIIET_ASPECT_HOME if exist "%NIIET_ASPECT_HOME%\riscv_kit_windows\bin\make.exe" set "MAKE_EXE=%NIIET_ASPECT_HOME%\riscv_kit_windows\bin\make.exe"
if not defined OPENOCD_EXE if defined NIIET_ASPECT_HOME if exist "%NIIET_ASPECT_HOME%\riscv_kit_windows\bin\openocd.exe" set "OPENOCD_EXE=%NIIET_ASPECT_HOME%\riscv_kit_windows\bin\openocd.exe"
if not defined OPENOCD_SCRIPTS_ROOT if defined NIIET_ASPECT_HOME if exist "%NIIET_ASPECT_HOME%\riscv_kit_windows\target" set "OPENOCD_SCRIPTS_ROOT=%NIIET_ASPECT_HOME%\riscv_kit_windows"
if not defined COMPILER_PATH if defined NIIET_ASPECT_HOME if exist "%NIIET_ASPECT_HOME%\riscv_gcc_windows\bin" set "COMPILER_PATH=%NIIET_ASPECT_HOME%\riscv_gcc_windows\bin"
if not defined GDB_EXE if defined COMPILER_PATH if exist "%COMPILER_PATH%\riscv64-unknown-elf-gdb.exe" set "GDB_EXE=%COMPILER_PATH%\riscv64-unknown-elf-gdb.exe"
if not defined OBJCOPY_EXE if defined COMPILER_PATH if exist "%COMPILER_PATH%\riscv64-unknown-elf-objcopy.exe" set "OBJCOPY_EXE=%COMPILER_PATH%\riscv64-unknown-elf-objcopy.exe"

if not defined MAKE_EXE (
  for /f "delims=" %%I in ('where make.exe 2^>nul') do (
    set "MAKE_EXE=%%I"
    goto :after_make
  )
)
:after_make

if not defined OPENOCD_EXE (
  for /f "delims=" %%I in ('where openocd.exe 2^>nul') do (
    set "OPENOCD_EXE=%%I"
    goto :after_openocd
  )
)
:after_openocd

if not defined GDB_EXE (
  for /f "delims=" %%I in ('where riscv64-unknown-elf-gdb.exe 2^>nul') do (
    set "GDB_EXE=%%I"
    goto :after_gdb
  )
)
:after_gdb

if not defined OBJCOPY_EXE (
  for /f "delims=" %%I in ('where riscv64-unknown-elf-objcopy.exe 2^>nul') do (
    set "OBJCOPY_EXE=%%I"
    goto :after_objcopy
  )
)
:after_objcopy

if not defined COMPILER_PATH (
  for /f "delims=" %%I in ('where riscv64-unknown-elf-gcc.exe 2^>nul') do (
    set "COMPILER_PATH=%%~dpI"
    goto :after_compiler
  )
)
:after_compiler

if not defined OPENOCD_SCRIPTS_ROOT if defined OPENOCD_EXE (
  for %%I in ("%OPENOCD_EXE%") do set "OPENOCD_SCRIPTS_ROOT=%%~dpI.."
)
if defined OPENOCD_SCRIPTS_ROOT if not exist "%OPENOCD_SCRIPTS_ROOT%\target" set "OPENOCD_SCRIPTS_ROOT="

set "_NIIET_ASPECT_HOME=%NIIET_ASPECT_HOME%"
set "_MAKE_EXE=%MAKE_EXE%"
set "_OPENOCD_EXE=%OPENOCD_EXE%"
set "_OPENOCD_SCRIPTS_ROOT=%OPENOCD_SCRIPTS_ROOT%"
set "_COMPILER_PATH=%COMPILER_PATH%"
set "_GDB_EXE=%GDB_EXE%"
set "_OBJCOPY_EXE=%OBJCOPY_EXE%"

endlocal & (
  if not "%_NIIET_ASPECT_HOME%"=="" set "NIIET_ASPECT_HOME=%_NIIET_ASPECT_HOME%"
  if not "%_MAKE_EXE%"=="" set "MAKE_EXE=%_MAKE_EXE%"
  if not "%_OPENOCD_EXE%"=="" set "OPENOCD_EXE=%_OPENOCD_EXE%"
  if not "%_OPENOCD_SCRIPTS_ROOT%"=="" set "OPENOCD_SCRIPTS_ROOT=%_OPENOCD_SCRIPTS_ROOT%"
  if not "%_COMPILER_PATH%"=="" set "COMPILER_PATH=%_COMPILER_PATH%"
  if not "%_GDB_EXE%"=="" set "GDB_EXE=%_GDB_EXE%"
  if not "%_OBJCOPY_EXE%"=="" set "OBJCOPY_EXE=%_OBJCOPY_EXE%"
)
exit /b 0
