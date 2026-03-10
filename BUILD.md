# Сборка и прошивка без VS Code

Этот проект можно собирать и прошивать из любого терминала под Windows
## Быстрый старт (Windows)

Из корня репозитория:

```bat
tools\build_bootloader.bat
tools\build_app.bat
```

OpenOCD и отладка:

```bat
tools\start_openocd.bat
tools\flash_bootloader.bat
tools\debug_bootloader_load.bat
tools\debug_app_attach.bat
```

Прошивка приложения через UART bootloader:

```bat
tools\flash_app_via_bootloader.bat
```

Интерактивное меню:

```bat
tools\dev_menu.bat
```

## Переменные toolchain

Значения по умолчанию можно переопределить через переменные окружения:

- `COMPILER_PATH` - каталог, где находится `riscv64-unknown-elf-gcc`
- `PREFIX` - префикс toolchain, по умолчанию `riscv64-unknown-elf-`
- `MARCH` - по умолчанию `rv32imfc_zicsr`
- `MABI` - по умолчанию `ilp32f`
- `OPTIMISATION` - по умолчанию `-O0`
- `MAKE_EXE` - путь к `make.exe`
- `OPENOCD_EXE` - путь к `openocd.exe`
- `OPENOCD_SCRIPTS_ROOT` - каталог скриптов OpenOCD
- `GDB_EXE` - путь к GDB
- `OBJCOPY_EXE` - путь к `objcopy`
- `PYTHON_EXE` - путь к Python
- `BOOT_PORT` - UART-порт для прошивки через bootloader (по умолчанию `auto`)
- `BOOT_BAUD` - скорость UART (по умолчанию `115200`)
- `BOOT_TIMEOUT` - таймаут скрипта bootloader в секундах (по умолчанию `30`)

Опциональная безопасная переменная для локальных проверок:

- `DRY_RUN=1` - печатает последовательность команд без выполнения

Пример (PowerShell):

```powershell
$env:COMPILER_PATH="C:/Users/kac/.niiet_aspect/riscv_gcc_windows/bin"
tools\build_app.bat
$env:DRY_RUN="1"
tools\flash_app_via_bootloader.bat
```

## Прошивка через UART Bootloader

```bat
python bootloader/boot.py --port auto --bin vg015/build/vg015.bin --baud 115200
```

