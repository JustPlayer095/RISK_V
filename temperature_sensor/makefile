#==============================================================================
# Makefile NIIET K1921VG015 - Temperature Sensor Project
# Created by NIIET
# free to use
#==============================================================================
TARGET_NAME ?= temperature_sensor
	
COMPILER_PATH ?=
ADDITIONAL_FLAGS ?=
DEFINES ?= -DRETARGET
PREFIX ?= 
	
LINKER_PATH  		?=
LD_SRC 				?=
MARCH				?=
MABI				?=
BUILD_NAME			?= build
BUILD_PATH  		:= ./$(BUILD_NAME)
	
OPTIMISATION ?=

rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
	
SRCS := $(call rwildcard,.,*.c)
ASMS := $(call rwildcard,.,*.S)
HEADERS := $(call rwildcard,.,*.h)
	
OBJS := $(SRCS:.c=.o)
OBJS += $(ASMS:.S=.o)
	
INCLUDE_FOLDERS = $(foreach inc, $(sort $(dir $(HEADERS))), -I"$(inc)") -I"./device/Include" -I"./plib/inc"
OBJ_DIRS = $(sort $(dir $(OBJS)))
	
LIBS ?= -L$(LINKER_PATH)
LIBS_ACR ?= -lc_nano -lg_nano -lgcc
	
LD_FLAGS ?= -static $(LIBS)  $(LIBS_ACR)
	
export CROSS_PREFIX  ?= $(COMPILER_PATH)/$(PREFIX)
ifeq ($(OS), Windows_NT)
export RISCV-GCC	 ?= $(CROSS_PREFIX)gcc.exe -c -Wall $(OPTIMISATION) -march=$(MARCH) -mabi=$(MABI) -g -ggdb3 $(DEFINES) $(INCLUDE_FOLDERS) -static -std=gnu99 $(LIBS) $(LIBS_ACR) $(ADDITIONAL_FLAGS)
export RISCV-AS 	 ?= $(CROSS_PREFIX)as.exe -mno-arch-attr $(OPTIMISATION) -march=$(MARCH) -mabi=$(MABI) $(ADDITIONAL_FLAGS)
export RISCV-LD 	 ?= $(CROSS_PREFIX)ld.exe 
export RISCV-OBJDUMP ?= $(CROSS_PREFIX)objdump.exe -S
export RISCV-OBJCOPY ?= $(CROSS_PREFIX)objcopy.exe
export RISCV-SIZE 	 ?= $(CROSS_PREFIX)size.exe
export RISCV-GDB	 ?= $(CROSS_PREFIX)gdb.exe
else 
export RISCV-GCC	 ?= $(CROSS_PREFIX)gcc -c -Wall $(OPTIMISATION) -g -ggdb3 -march=$(MARCH) -mabi=$(MABI) $(DEFINES) $(INCLUDE_FOLDERS) -static -std=gnu99 $(LIBS) $(LIBS_ACR) $(ADDITIONAL_FLAGS)
export RISCV-AS 	 ?= $(CROSS_PREFIX)as -mno-arch-attr $(OPTIMISATION) -march=$(MARCH) -mabi=$(MABI) $(ADDITIONAL_FLAGS)
export RISCV-LD 	 ?= $(CROSS_PREFIX)ld 
export RISCV-OBJDUMP ?= $(CROSS_PREFIX)objdump -S
export RISCV-OBJCOPY ?= $(CROSS_PREFIX)objcopy
export RISCV-SIZE 	 ?= $(CROSS_PREFIX)size
export RISCV-GDB	 ?= $(CROSS_PREFIX)gdb
endif

ifeq ($(OS), Windows_NT)
make_build:
	@ -$(foreach dir, $(OBJ_DIRS), mkdir "$(BUILD_PATH)/$(dir)" && ):
else
make_build:
	@ -$(foreach dir,$(OBJ_DIRS),mkdir -p $(BUILD_PATH)/$(dir) && ):
endif
	
help:
	@ echo "help: make [all, clean, load]"
	
header:
	@ echo "NIIET MAKEFILE v.1.0 - Temperature Sensor Project"
	
%.o: %.S
	@ echo "ASSEMBLY "$^"" 
	$(RISCV-GCC) $^ -o $(BUILD_PATH)/$@
	@ echo "...DONE"
	
%.o: %.c
	@ echo "COMPILE "$^""
	$(RISCV-GCC) $^ -o $(BUILD_PATH)/$@ 
	@ echo "...DONE"
	
%.elf: $(OBJS)
	@ echo "LINK "$^"" 
	$(CROSS_PREFIX)g++ -o $@ $(foreach obj, $(OBJS), $(BUILD_PATH)/$(obj)) -static -march=$(MARCH) -mabi=$(MABI) -g -ggdb3 -std=gnu99 $(LD_FLAGS) -nostartfiles -T ./device/ldscripts/k1921vg015_flash.ld
	@ echo "...DONE" 
	
%.dump: %.elf
	@ echo "DUMP "$^"" 
	$(RISCV-OBJDUMP) $^ > $@
	@ echo "...DONE"
	
size: $(BUILD_PATH)/$(TARGET_NAME).elf
	@ echo "~~SIZE~~ """ 
	$(RISCV-SIZE) $^
	
clean:
	@ echo "CLEANING "$^""
	@ -if exist $(BUILD_PATH) rmdir /s /q $(BUILD_PATH)
	@ echo "...DONE"
	
hex:
	@ echo "HEX "
	$(RISCV-OBJCOPY) -O ihex $(BUILD_PATH)/$(TARGET_NAME).elf $(BUILD_PATH)/$(TARGET_NAME).hex
	@ echo "...DONE"
	

all: header clean make_build $(BUILD_PATH)/$(TARGET_NAME).elf $(BUILD_PATH)/$(TARGET_NAME).dump hex size
	
	
debug: all
	@ echo "LOAD "$(TARGET_NAME).elf"" 
	$(RISCV-GDB) $(BUILD_PATH)/$(TARGET_NAME).elf -iex="target extended-remote localhost:3333" -ex="monitor reset halt" -ex="load"
