# STM32 智能农业驱离装置 — Makefile
# 硬件: STM32F103C8T6 (Blue Pill)
# 工具链: ARM GCC (arm-none-eabi-gcc)
# 
# 编译: make
# 烧录: make flash (需要st-flash或openocd)
# 清理: make clean

# 项目名称
TARGET = bio_control
BUILD_DIR = build

# 工具链
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
OD = $(PREFIX)objdump
GDB = $(PREFIX)gdb

# CPU参数
CPU = -mcpu=cortex-m3
MCU = $(CPU) -mthumb -mfloat-abi=soft

# 芯片定义
DEFS = -DSTM32F103xB -DUSE_HAL_DRIVER

# HAL库路径 (需要单独下载STM32CubeF1)
HAL_DIR = ./Drivers/STM32F1xx_HAL_Driver
CMSIS_DIR = ./Drivers/CMSIS

# 编译选项
CFLAGS = $(MCU) $(DEFS)
CFLAGS += -Wall -Wextra -Wshadow
CFLAGS += -O2 -g3
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fno-common -fmessage-length=0
CFLAGS += -std=gnu11

# 包含路径
INCLUDES = -I./Core/Inc
INCLUDES += -I$(HAL_DIR)/Inc
INCLUDES += -I$(CMSIS_DIR)/Device/ST/STM32F1xx/Include
INCLUDES += -I$(CMSIS_DIR)/Include

# 链接脚本
LDSCRIPT = STM32F103C8Tx_FLASH.ld

# 链接选项
LDFLAGS = $(MCU) -specs=nano.specs -specs=nosys.specs
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref
LDFLAGS += -Wl,--gc-sections

# 源文件
C_SOURCES = \
Core/Src/main.c \
Core/Src/buzzer.c \
Core/Src/strobe.c \
Core/Src/serial_comm.c \
Core/Src/sensor.c

# 启动文件
ASM_SOURCES = startup_stm32f103xb.s

# 自动生成对象文件列表
C_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
ASM_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))

# 构建目标
all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# 链接
$(BUILD_DIR)/$(TARGET).elf: $(C_OBJS) $(ASM_OBJS)
	@echo "🔗 链接 $@"
	$(CC) $(C_OBJS) $(ASM_OBJS) $(LDFLAGS) -o $@
	$(SZ) $@

# 编译C文件
$(BUILD_DIR)/%.o: Core/Src/%.c
	@echo "🔧 编译 $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: Core/Src/%.s
	@echo "🔧 汇编 $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 生成HEX (用于ST-Link/串口烧录)
$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	@echo "📦 生成HEX $@"
	$(CP) -O ihex $< $@

# 生成BIN (用于DFU/自定义Bootloader)
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	@echo "📦 生成BIN $@"
	$(CP) -O binary $< $@

# 生成反汇编 (调试用)
$(BUILD_DIR)/$(TARGET).lst: $(BUILD_DIR)/$(TARGET).elf
	$(OD) -S $< > $@

# 烧录 (ST-Link)
flash: $(BUILD_DIR)/$(TARGET).bin
	@echo "🔥 烧录到STM32..."
	st-flash --reset write $< 0x08000000

# 烧录 (OpenOCD)
flash-ocd: $(BUILD_DIR)/$(TARGET).bin
	@echo "🔥 烧录(OpenOCD)..."
	openocd -f interface/stlink.cfg \
	        -f target/stm32f1x.cfg \
	        -c "program $< 0x08000000 verify reset exit"

# 烧录 (串口 — 使用stm32flash)
flash-serial: $(BUILD_DIR)/$(TARGET).bin
	@echo "🔥 串口烧录..."
	stm32flash -w $< -v -g 0x0 /dev/ttyUSB0

# 调试 (GDB + OpenOCD)
debug:
	@echo "🐛 启动GDB调试..."
	$(GDB) $(BUILD_DIR)/$(TARGET).elf \
		-ex "target extended-remote :3333" \
		-ex "monitor reset halt" \
		-ex "load" \
		-ex "monitor reset init"

# 清理
clean:
	@echo "🧹 清理构建文件..."
	rm -rf $(BUILD_DIR)

# 格式化代码
format:
	@echo "📝 格式化代码..."
	clang-format -i Core/Src/*.c Core/Inc/*.h

.PHONY: all flash flash-ocd flash-serial debug clean format
