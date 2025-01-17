# Project configuration
PROJECT_DIR := $(CURDIR)
BUILD_DIR := $(PROJECT_DIR)/build
BOARD := nrf9160dk_nrf9160_ns  # Replace with your board

# Paths
NRF_TOOLCHAIN := ../../ncs
NRF_UTIL := ${HOME}/.local/bin/nrfutil
# Build commake
NRF=$(NRF_UTIL) toolchain-manager launch  --install-dir $(NRF_TOOLCHAIN) --

# Targets
default: build

build:
	@echo "Building the project for board $(BOARD)..."
	$(NRF) west build --sysbuild -b $(BOARD) -d $(BUILD_DIR) $(PROJECT_DIR)

flash:
	@echo "Flashing the board..."
	$(NRF) west flash -d $(BUILD_DIR)

clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)

menuconfig:
	@echo "Running menuconfig..."
	$(NRF) west build --sysbuild -t menuconfig -d $(BUILD_DIR)

# Phony targets
.PHONY: default build flash clean menuconfig

