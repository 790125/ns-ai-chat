#---------------------------------------------------------------------------------
# Ns_Practice — Switch AI Chat Homebrew (.nro)
# 构建: 在 devkitPro MSYS2 环境中执行 make
#---------------------------------------------------------------------------------

# ---- 项目配置 ----
TARGET      := Ns_Deep_Ai
BUILD       := build_switch
APP_TITLE   := "AI Chat (DeepSeek)"
APP_AUTHOR  := Ns_Practice
APP_VERSION := 1.0.0
ICON        := icon.jpg
ROMFSDIR    := romfs

# ---- 工具链 ----
DEVKITPRO   ?= /opt/devkitpro
PORTLIBS    := $(DEVKITPRO)/portlibs/switch
LIBNX       := $(DEVKITPRO)/libnx

PREFIX      := aarch64-none-elf-
CC          := $(PREFIX)gcc
CXX         := $(PREFIX)g++
LD          := $(PREFIX)g++
OBJCOPY     := $(PREFIX)objcopy

# ---- 编译选项 ----
ARCH        := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

INCLUDES    := -I$(PORTLIBS)/include \
               -I$(PORTLIBS)/include/SDL2 \
               -I$(LIBNX)/include \
               -I.

DEFINES     := -D__SWITCH__

CXXFLAGS    := -g -Wall -O2 -std=c++17 -ffunction-sections \
               -fdata-sections $(ARCH) $(DEFINES) $(INCLUDES)

LDFLAGS     := -specs=$(LIBNX)/switch.specs $(ARCH) \
               -Wl,-Map=$(BUILD)/$(TARGET).map \
               -L$(PORTLIBS)/lib -L$(LIBNX)/lib

LIBS        := -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image \
               -lcurl \
               -lfreetype -lharfbuzz -lpng16 -ljpeg -lwebp -lz -lbz2 \
               -lEGL -lglapi -ldrm_nouveau \
               -lnx -lm -lstdc++

# ---- 源文件与目标文件 ----
CPPFILES    := Ns_Practice.cpp DeepSeekClient.cpp Deepseek_Running_Logite.cpp
OFILES      := $(addprefix $(BUILD)/, $(CPPFILES:.cpp=.o))
DEPS        := $(OFILES:.o=.d)

ELF         := $(BUILD)/$(TARGET).elf
NRO         := $(TARGET).nro
NACP        := $(TARGET).nacp

# ---- 默认目标 ----
.PHONY: all clean
all: $(NRO)

# ---- .nro: ELF → NRO (含 RomFS 嵌入) ----
$(NRO): $(ELF) $(NACP)
	@echo "  NRO     $@"
	@elf2nro $(ELF) $@ --icon=$(ICON) --nacp=$(NACP) --romfsdir=$(ROMFSDIR)
	@echo "  => $(NRO) built successfully ($(shell du -sh $@ 2>/dev/null || wc -c < $@) bytes)"

# ---- .elf: 链接所有 .o ----
$(ELF): $(OFILES)
	@echo "  LD      $(notdir $@)"
	@$(LD) $(LDFLAGS) $(OFILES) $(LIBS) -o $@

# ---- .nacp: 元数据 ----
$(NACP):
	@echo "  NACP    $@"
	@nacptool --create $(APP_TITLE) $(APP_AUTHOR) $(APP_VERSION) $@

# ---- .o: 编译 .cpp ----
$(BUILD)/%.o: %.cpp Makefile
	@mkdir -p $(BUILD)
	@echo "  CXX     $(notdir $<)"
	@$(CXX) -MMD -MP -MF $(BUILD)/$*.d $(CXXFLAGS) -c $< -o $@

# ---- 包含自动生成的依赖文件 ----
-include $(DEPS)

# ---- 清理 ----
clean:
	@rm -rf $(BUILD) $(NRO) $(NACP) *.elf
	@echo "  cleaned."
