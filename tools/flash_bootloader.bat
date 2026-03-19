@echo off
setlocal EnableDelayedExpansion

set "ROOT=%~dp0.."
call "%~dp0toolchain_env.bat"
if "%GDB_EXE%"=="" (
  echo ERROR: GDB_EXE not found. Set GDB_EXE or add riscv64-unknown-elf-gdb.exe to PATH.
  exit /b 1
)
set "BOOTLOADER_ELF=%BOOTLOADER_ELF%"
if "%BOOTLOADER_ELF%"=="" set "BOOTLOADER_ELF=%ROOT%\bootloader\build\bootloader.elf"
set "AUTO_START_OPENOCD=%AUTO_START_OPENOCD%"
if "%AUTO_START_OPENOCD%"=="" set "AUTO_START_OPENOCD=1"

if "%DRY_RUN%"=="1" (
  echo DRY RUN: call "%ROOT%\tools\build_bootloader.bat"
  echo DRY RUN: if OpenOCD on :3333 is not available and AUTO_START_OPENOCD=1, start "%ROOT%\tools\start_openocd.bat"
  echo DRY RUN: "%GDB_EXE%" "%BOOTLOADER_ELF%" -q -batch -ex="set confirm off" -ex="target extended-remote localhost:3333" -ex="monitor reset halt" -ex="load" -ex="monitor reset run" -ex="disconnect" -ex="quit"
  exit /b 0
)

call "%ROOT%\tools\build_bootloader.bat"
if errorlevel 1 exit /b %ERRORLEVEL%

if "%AUTO_START_OPENOCD%"=="1" (
  set "PORT_OPEN=0"
  for /f %%R in ('powershell -NoProfile -Command "$x=Get-NetTCPConnection -LocalPort 3333 -State Listen -ErrorAction SilentlyContinue; if($x){Write-Output 1}else{Write-Output 0}"') do set "PORT_OPEN=%%R"
  if not "!PORT_OPEN!"=="1" (
    echo OpenOCD is not running on localhost:3333. Starting OpenOCD in separate window...
    powershell -NoProfile -Command "Start-Process -FilePath 'cmd.exe' -ArgumentList '/d','/c','\"\"%ROOT%\tools\start_openocd.bat\"\"' -WindowStyle Minimized" >nul 2>nul
  )
)

set "OPENOCD_READY="
for /L %%I in (1,1,30) do (
  set "PORT_OPEN=0"
  for /f %%R in ('powershell -NoProfile -Command "$x=Get-NetTCPConnection -LocalPort 3333 -State Listen -ErrorAction SilentlyContinue; if($x){Write-Output 1}else{Write-Output 0}"') do set "PORT_OPEN=%%R"
  if "!PORT_OPEN!"=="1" (
    set "OPENOCD_READY=1"
    goto :openocd_ready
  )
  timeout /t 1 /nobreak >nul
)

:openocd_ready
if not "%OPENOCD_READY%"=="1" (
  echo ERROR: OpenOCD did not become ready on localhost:3333.
  exit /b 1
)

set "GDB_RC=1"
for /L %%I in (1,1,5) do (
  set "GDB_LOG=%TEMP%\gdb_bootloader_%%I_%RANDOM%.log"
  "%GDB_EXE%" "%BOOTLOADER_ELF%" -q -batch -ex="set confirm off" -ex="target extended-remote localhost:3333" -ex="monitor reset halt" -ex="load" -ex="monitor reset run" -ex="disconnect" -ex="quit" > "!GDB_LOG!" 2>&1
  set "GDB_RC=!ERRORLEVEL!"
  type "!GDB_LOG!"
  findstr /C:"Connection timed out." /C:"command not supported by this target." "!GDB_LOG!" >nul
  if not errorlevel 1 set "GDB_RC=1"
  del /q "!GDB_LOG!" >nul 2>nul
  if "!GDB_RC!"=="0" goto :gdb_done
  timeout /t 1 /nobreak >nul
)

:gdb_done
exit /b %GDB_RC%
