# Builds the panel-refresh firmware against a checkout of OpenEPaperLink's Tag_FW_Chroma
# (https://github.com/OpenEPaperLink/Tag_FW_Chroma), with SDCC on PATH:
#   make                      -> chroma29_panel_refresh.bin (40 cycles, a test pattern every 10)
#   make CYCLES=20 TEST_EVERY=5
#   (FW is required: make FW=/path/to/Tag_FW_Chroma/Chroma_Tag_FW)
CYCLES ?= 40
TEST_EVERY ?= 10
FW ?= $(error set FW to your Tag_FW_Chroma/Chroma_Tag_FW checkout, e.g. make FW=/path/to/Tag_FW_Chroma/Chroma_Tag_FW)
DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
UV := uv run --no-project --with pillow python -I

all:
	cd $(DIR) && $(UV) render_refresh.py frames $(CYCLES) $(TEST_EVERY)
	cd $(DIR) && $(UV) make_images.py frames.h $$(ls frames/*.png | sort -n -t/ -k2)
	printf '#define CYCLES %s\n#define TEST_EVERY %s\n' $(CYCLES) $(TEST_EVERY) >>$(DIR)/frames.h
	mkdir -p $(FW)/PanelRefresh
	cp $(DIR)/panel_refresh.c $(DIR)/demo_stubs.c $(DIR)/frames.h $(DIR)/refresh.mk $(FW)/PanelRefresh/
	rm -rf $(FW)/builds/panel_refresh
	$(MAKE) -C $(FW)/PanelRefresh -f refresh.mk
	cp $(FW)/builds/panel_refresh/chroma29_panel_refresh.bin $(DIR)/chroma29_panel_refresh.bin

clean:
	rm -rf $(DIR)/frames $(DIR)/frames.h

.PHONY: all clean
