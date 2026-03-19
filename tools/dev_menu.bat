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
echo   3^) Flash bootloader ^(auto-start OpenOCD^)
echo   4^) Flash app via UART bootloader
echo   5^) Debug bootloader ^(load, auto-start OpenOCD^)
echo   6^) Debug app ^(attach, auto-start OpenOCD^)
echo   7^) Dry-run flash app script
echo   8^) Exit
echo.
set /p CHOICE=Select action [1-8]:

if "%CHOICE%"=="1" call "%~dp0build_bootloader.bat" & goto done
if "%CHOICE%"=="2" call "%~dp0build_app.bat" & goto done
if "%CHOICE%"=="3" call "%~dp0flash_bootloader.bat" & goto done
if "%CHOICE%"=="4" call "%~dp0flash_app_via_bootloader.bat" & goto done
if "%CHOICE%"=="5" call "%~dp0debug_bootloader_load.bat" & goto done
if "%CHOICE%"=="6" call "%~dp0debug_app_attach.bat" & goto done
if "%CHOICE%"=="7" set DRY_RUN=1 & call "%~dp0flash_app_via_bootloader.bat" & set DRY_RUN= & goto done
if "%CHOICE%"=="8" goto end

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
