# Directory Definitions
SRC_DIR   := src
BUILD_DIR := build

# DYNAMICALLY FIND SYSTEM PLUGIN PATH
# This asks GStreamer where plugins should live on this specific OS.
# On Ubuntu, this is usually: /usr/lib/x86_64-linux-gnu/gstreamer-1.0
INSTALL_DIR := $(shell pkg-config --variable=pluginsdir gstreamer-1.0)

# Fallback: If pkg-config fails to find the variable, default to standard location
ifeq ($(INSTALL_DIR),)
    ARCH := $(shell dpkg-architecture -qDEB_HOST_MULTIARCH)
    INSTALL_DIR := /usr/lib/$(ARCH)/gstreamer-1.0
endif

# Target Name
LIB_NAME  := libgstmorsesrc.so
TARGET    := $(BUILD_DIR)/$(LIB_NAME)
CONFIG_H  := $(BUILD_DIR)/config.h

# Compiler Settings
CC := gcc
PKG_CONFIG_DEPS := gstreamer-1.0 gstreamer-base-1.0 gstreamer-audio-1.0

# Flags - Include build directory for config.h
CFLAGS := -Wall -fPIC -O2 -I$(BUILD_DIR) $(shell pkg-config --cflags $(PKG_CONFIG_DEPS))
LIBS   := $(shell pkg-config --libs $(PKG_CONFIG_DEPS)) -lm

# Source Files
SOURCES := $(wildcard $(SRC_DIR)/*.c)

# Default Rule
all: $(TARGET)

# Create Build Directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Generate config.h with BUILD_DATE
$(CONFIG_H): | $(BUILD_DIR)
	@echo "Generating config.h with BUILD_DATE..."
	@echo "#ifndef __CONFIG_H__" > $@
	@echo "#define __CONFIG_H__" >> $@
	@echo "" >> $@
	@echo "#define BUILD_DATE \"$(shell date '+%Y-%m-%d')\"" >> $@
	@echo "" >> $@
	@echo "#endif /* __CONFIG_H__ */" >> $@
	@echo "✓ Config header generated (BUILD_DATE: $(shell date '+%Y-%m-%d'))"

# Link Rule - Depends on config.h being generated first
$(TARGET): $(CONFIG_H) $(SOURCES)
	@echo "Building $@..."
	$(CC) $(CFLAGS) -shared -o $@ $(SOURCES) $(LIBS)
	@echo "✓ Build complete: $@"

# Install Rule (Requires Root/Sudo)
install: $(TARGET)
	@echo "Installing to SYSTEM directory: $(INSTALL_DIR)"
	@echo "Note: If this fails, run with 'sudo make install'"
	@install -m 755 -d $(INSTALL_DIR)
	@install -m 644 $(TARGET) $(INSTALL_DIR)
	@echo "✓ Installation Complete. Run 'gst-inspect-1.0 morsesrc' to verify."

# Clean Rule
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)
	@echo "✓ Clean complete"

# Uninstall Rule (Requires Root/Sudo)
uninstall:
	@echo "Removing $(LIB_NAME) from $(INSTALL_DIR)..."
	@rm -f $(INSTALL_DIR)/$(LIB_NAME)
	@echo "✓ Uninstall complete"

# Show configuration info
info:
	@echo "=== morsesrc Build Configuration ==="
	@echo "Source Directory: $(SRC_DIR)"
	@echo "Build Directory: $(BUILD_DIR)"
	@echo "Install Directory: $(INSTALL_DIR)"
	@echo "Compiler: $(CC)"
	@echo "Build Date: $(shell date '+%Y-%m-%d')"
	@echo ""
	@echo "Sources: $(SOURCES)"
	@echo ""
	@echo "CFLAGS: $(CFLAGS)"
	@echo ""
	@echo "LIBS: $(LIBS)"

# Verify plugin after install
verify:
	@echo "Verifying installation..."
	@gst-inspect-1.0 morsesrc || echo "Plugin not found. Try: gst-plugin-scanner"

# Complete rebuild
rebuild: clean all
	@echo "✓ Rebuild complete"

.PHONY: all install clean uninstall info verify rebuild
