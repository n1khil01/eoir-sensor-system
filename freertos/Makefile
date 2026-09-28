# eoir-sensor-system — Week 6 FreeRTOS module
# Builds a standalone FreeRTOS image for QEMU's mps2-an385 machine
# (Cortex-M3). No STM32 hardware required — see app/main.c for what this
# demonstrates and why.

OUTPUT_DIR := build
IMAGE      := $(OUTPUT_DIR)/rtos_week6.elf

CC   = arm-none-eabi-gcc
LD   = arm-none-eabi-gcc
SIZE = arm-none-eabi-size

KERNEL_DIR      = kernel
KERNEL_PORT_DIR = $(KERNEL_DIR)/portable/GCC/ARM_CM3
APP_DIR         = app

INCLUDE_DIRS = -I$(KERNEL_DIR)/include -I$(KERNEL_PORT_DIR) -I$(APP_DIR) -I$(APP_DIR)/CMSIS

CFLAGS  = -mthumb -mcpu=cortex-m3 -ffreestanding
CFLAGS += -g3 -O1 -ffunction-sections -fdata-sections
CFLAGS += -Wall -Wextra -Wshadow -Wno-unused-parameter
CFLAGS += $(INCLUDE_DIRS)

LDFLAGS  = -T $(APP_DIR)/mps2_m3.ld
LDFLAGS += -Xlinker -Map=$(OUTPUT_DIR)/rtos_week6.map
LDFLAGS += -Xlinker --gc-sections
LDFLAGS += -nostartfiles -specs=nano.specs -specs=nosys.specs

SOURCE_FILES += $(KERNEL_DIR)/tasks.c
SOURCE_FILES += $(KERNEL_DIR)/list.c
SOURCE_FILES += $(KERNEL_DIR)/queue.c
SOURCE_FILES += $(KERNEL_DIR)/timers.c
SOURCE_FILES += $(KERNEL_DIR)/event_groups.c
SOURCE_FILES += $(KERNEL_DIR)/portable/MemMang/heap_4.c
SOURCE_FILES += $(KERNEL_PORT_DIR)/port.c
SOURCE_FILES += $(APP_DIR)/main.c
SOURCE_FILES += $(APP_DIR)/startup_gcc.c
SOURCE_FILES += $(APP_DIR)/printf-stdarg.c

OBJS = $(addprefix $(OUTPUT_DIR)/,$(notdir $(SOURCE_FILES:.c=.o)))
VPATH = $(sort $(dir $(SOURCE_FILES)))

.PHONY: all clean run

all: $(IMAGE)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(OUTPUT_DIR)/%.o: %.c | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(IMAGE): $(APP_DIR)/mps2_m3.ld $(OBJS) | $(OUTPUT_DIR)
	$(LD) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(IMAGE)
	$(SIZE) $(IMAGE)

# Run headless. -icount decouples the guest's virtual clock from host
# wall-clock time (see app/main.c's top comment) -- without it, the reported
# jitter/latency numbers measure host OS scheduling noise on the QEMU
# process, not the guest's own timing. Capped to 120s wall time in case the
# build hangs; a normal run finishes and halts well before that.
TIMEOUT := $(shell command -v timeout || command -v gtimeout)

run: $(IMAGE)
	$(TIMEOUT) 120 qemu-system-arm -machine mps2-an385 -cpu cortex-m3 \
		-icount shift=auto \
		-kernel $(IMAGE) -monitor none -nographic -serial stdio \
		|| true

clean:
	rm -rf $(OUTPUT_DIR)
