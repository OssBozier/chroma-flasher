# Copied into Tag_FW_Chroma/Chroma_Tag_FW/PanelRefresh by the Makefile beside it
BUILD = chroma29
BOARD = chroma29
SCREEN_SRC = screen.c
FLAGS += -DBUILD=$(BUILD) -DPANEL_REFRESH

#file containing main() must be first!
SOURCES = panel_refresh.c demo_stubs.c drawing.c cpu.c board.c powermgt.c draw_common.c eeprom.c settings.c
include ../make/common.mk
VPATH += $(FIRMWARE_ROOT)/OEPL
FLAGS += -I$(FIRMWARE_ROOT)/OEPL
SOURCES += $(SOC_DIR)/wdt.c $(SOC_DIR)/timer.c $(SOC_DIR)/sleep.c
SOURCES += $(SOC_DIR)/adc.c $(SOC_DIR)/soc.c $(SOC_DIR)/printf.c
SOURCES += board.c $(SCREEN_SRC)
FLAGS += -I. -MD

BUILD_DIR = $(BUILDS_DIR)/panel_refresh
IMAGE_NAME = chroma29_panel_refresh
include $(FIRMWARE_ROOT)/make/rules.mk
