@echo off
setlocal
title WORKPROJECT Dev Menu

:menu
cls
echo ==========================================
echo   WORKPROJECT - Build / Flash / Debug
echo ==========================================
echo.
echo   1^) Build bootloader
echo   2^) Build app
echo   3^) Start OpenOCD
echo   4^) Flash bootloader ^(GDB/OpenOCD^)
echo   5^) Flash app via UART bootloader
echo   6^) Debug bootloader ^(load^)
echo   7^) Debug app ^(attach^)
echo   8^) Dry-run flash app script
echo   9^) Exit
echo.
set /p CHOICE=Select action [1-9]:

if "%CHOICE%"=="1" call "%~dp0build_bootloader.bat" & goto done
if "%CHOICE%"=="2" call "%~dp0build_app.bat" & goto done
if "%CHOICE%"=="3" call "%~dp0start_openocd.bat" & goto done
if "%CHOICE%"=="4" call "%~dp0flash_bootloader.bat" & goto done
if "%CHOICE%"=="5" call "%~dp0flash_app_via_bootloader.bat" & goto done
if "%CHOICE%"=="6" call "%~dp0debug_bootloader_load.bat" & goto done
if "%CHOICE%"=="7" call "%~dp0debug_app_attach.bat" & goto done
if "%CHOICE%"=="8" set DRY_RUN=1 & call "%~dp0flash_app_via_bootloader.bat" & set DRY_RUN= & goto done
if "%CHOICE%"=="9" goto end

echo Invalid choice.
pause
goto menu

:done
echo.
echo Done. Exit code: %ERRORLEVEL%
echo.
pause
goto menu

:end
endlocal
exit /b 0
