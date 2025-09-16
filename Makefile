# ========================================
# Project Configuration
# ========================================
PROJECT_NAME := nsh
VERSION := 0.3.8

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
LIB_DIR := lib
EXTERNAL_LIB_DIR := external

# ========================================
# Build Type and Installation Path
# ========================================
BUILD_TYPE ?= release
PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin

# ========================================
# Library Management
# ========================================
LIBRARY_NEED ?= readline
MANUAL_LIBS ?=

# ========================================
# Compiler and Platform Detection
# ========================================
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(OS),Windows_NT)
    PLATFORM := Windows
else ifneq ($(filter MINGW MSYS,$(UNAME_S)),)
    PLATFORM := Windows
else
    PLATFORM := $(UNAME_S)
endif

ifeq ($(UNAME_M),x86_64)
    ARCH := x64
else ifneq ($(filter %86,$(UNAME_M)),)
    ARCH := x86
else ifneq ($(filter arm aarch64,$(UNAME_M)),)
    ARCH := $(UNAME_M)
else
    ARCH := unknown
endif

ifeq ($(PLATFORM),Linux)
    CXX := $(shell command -v g++ clang++ c++ 2>/dev/null | head -n 1)
    CC := $(shell command -v gcc clang cc 2>/dev/null | head -n 1)
else
    CXX := $(shell command -v clang++ g++ c++ 2>/dev/null | head -n 1)
    CC := $(shell command -v clang gcc cc 2>/dev/null | head -n 1)
endif

ifeq ($(CXX),)
$(error "No C++ compiler found.")
endif

ifeq ($(CC),)
	@echo "Warning: No C compiler found, using C++ compiler for C files."
	CC := $(CXX)
endif

COMPILER_TYPE := $(shell $(CXX) --version 2>/dev/null | head -n 1 | awk '{print tolower($$1)}')

# ========================================
# Flags Configuration
# ========================================
CXXFLAGS_BASE := -std=c++17 -Wall -Wextra -Wpedantic
CFLAGS_BASE := -std=c11 -Wall -Wextra

ifeq ($(BUILD_TYPE),debug)
    OPTIMIZATION := -O0 -g
    DEBUG_FLAGS := -DDEBUG
    LDFLAGS_EXTRA :=
    STRIP_FLAG :=
else ifeq ($(BUILD_TYPE),minsize)
    OPTIMIZATION := -Os -flto
    DEBUG_FLAGS := -DNDEBUG
    LDFLAGS_EXTRA := -flto -Wl,--gc-sections -Wl,--strip-all
    SIZE_FLAGS := -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables
    STRIP_FLAG :=
else # release
    OPTIMIZATION := -O3 -flto
    DEBUG_FLAGS := -DNDEBUG
    LDFLAGS_EXTRA := -flto -Wl,--gc-sections
    SIZE_FLAGS := -ffunction-sections -fdata-sections
    STRIP_FLAG :=
endif

ifeq ($(PLATFORM),Darwin)
    PLATFORM_FLAGS := -mmacosx-version-min=10.14
    LDFLAGS_PLATFORM :=
else ifeq ($(PLATFORM),Linux)
    PLATFORM_FLAGS := -D_GNU_SOURCE
    LDFLAGS_PLATFORM := -ldl -lrt
else ifeq ($(PLATFORM),Windows)
    PLATFORM_FLAGS := -D_WIN32_WINNT=0x0600 -DWIN32_LEAN_AND_MEAN
    LDFLAGS_PLATFORM := -static
endif

ifeq ($(COMPILER_TYPE),clang)
    COMPILER_FLAGS := -fcolor-diagnostics
else ifeq ($(COMPILER_TYPE),gcc)
    COMPILER_FLAGS := -fdiagnostics-color=always
else
    COMPILER_FLAGS :=
endif

CXXFLAGS := $(CXXFLAGS_BASE) $(OPTIMIZATION) $(SIZE_FLAGS) $(DEBUG_FLAGS) $(PLATFORM_FLAGS) $(COMPILER_FLAGS)
CFLAGS   := $(CFLAGS_BASE) $(OPTIMIZATION) $(SIZE_FLAGS) $(DEBUG_FLAGS) $(PLATFORM_FLAGS) $(COMPILER_FLAGS)
LDFLAGS  := $(OPTIMIZATION) $(LDFLAGS_EXTRA) $(LDFLAGS_PLATFORM)

# ========================================
# Dynamic Library and Environment Handling
# ========================================
HAS_PKG_CONFIG := $(shell command -v pkg-config 2>/dev/null)

# Function to get CFLAGS and LDFLAGS for a library
# Usage: $(call get_lib_flags,libname)
define get_lib_flags
$(if $(HAS_PKG_CONFIG),\
    $(if $(shell pkg-config --exists $(1) 2>/dev/null && echo 1), \
        $(info "  [$(1)] Detected with pkg-config.")\
        $(shell pkg-config --cflags $(1)) $(shell pkg-config --libs $(1)),\
        $(info "  [$(1)] pkg-config not found, using -l$(1).")\
        -l$(1)\
    ),\
    $(info "  [$(1)] pkg-config not found on system, using -l$(1).")\
    -l$(1)\
)
endef

# Process all required libraries
ALL_LIB_FLAGS := $(foreach lib,$(LIBRARY_NEED),$(call get_lib_flags,$(lib)))
LIB_CFLAGS := $(patsubst -I%,-I%,$(filter -I%,$(ALL_LIB_FLAGS)))
LIB_LDFLAGS := $(filter-out -I%,$(ALL_LIB_FLAGS)) $(MANUAL_LIBS)

# Handle dynamic environment variables, e.g., CPATH
INCLUDES := -I$(INC_DIR) -I$(LIB_DIR) -I$(EXTERNAL_LIB_DIR) $(LIB_CFLAGS) -Ibuiltins
# Example: Convert CPATH env var to -I flags, split by ':'
ifeq ($(CPATH),)
    # CPATH is empty or not set
else
    INCLUDES += $(patsubst %,-I%,$(subst :, ,$(CPATH)))
endif

# ========================================
# File and Directory Management
# ========================================
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
LIB_DIR := lib
EXTERNAL_LIB_DIR := external
TARGET := $(PROJECT_NAME)
ifeq ($(PLATFORM),Windows)
    TARGET := $(TARGET).exe
endif

SOURCES_CPP := $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/**/*.cpp)
SOURCES_C   := $(wildcard $(LIB_DIR)/*.c $(LIB_DIR)/**/*.c) \
               $(wildcard $(EXTERNAL_LIB_DIR)/*.c $(EXTERNAL_LIB_DIR)/**/*.c)

OBJECTS_CPP := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES_CPP))
OBJECTS_C   := $(patsubst $(LIB_DIR)/%.c,$(OBJ_DIR)/lib/%.o,$(filter $(LIB_DIR)/%,$(SOURCES_C))) \
               $(patsubst $(EXTERNAL_LIB_DIR)/%.c,$(OBJ_DIR)/external/%.o,$(filter $(EXTERNAL_LIB_DIR)/%,$(SOURCES_C)))
OBJECTS := $(OBJECTS_CPP) $(OBJECTS_C)

# ========================================
# Main Build Targets
# ========================================
.PHONY: all clean distclean install uninstall release debug minsize info with-libs format strip_target

all: $(BUILD_DIR) $(OBJ_DIR) $(TARGET)

$(BUILD_DIR) $(OBJ_DIR):
	@mkdir -p $(BUILD_DIR) $(OBJ_DIR)

$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET) for $(PLATFORM)-$(ARCH) with $(CXX)..."
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/$(TARGET) $^ $(LDFLAGS) $(LIB_LDFLAGS)
	@echo "Build complete: $(BUILD_DIR)/$(TARGET) ($(shell du -h $(BUILD_DIR)/$(TARGET) | awk '{print $$1}'))"

# ========================================
# Compilation Rules
# ========================================
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling C++ file: $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/lib/%.o: $(LIB_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling C library file: $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/external/%.o: $(EXTERNAL_LIB_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling C external library file: $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ========================================
# Utility Targets
# ========================================
release:
	$(MAKE) all BUILD_TYPE=release

debug:
	$(MAKE) all BUILD_TYPE=debug

minsize:
	$(MAKE) all BUILD_TYPE=minsize

with-libs:
	$(MAKE) LIBRARY_NEED="$(LIBS)"

clean:
	@echo "Cleaning up build artifacts..."
	@rm -rf $(OBJ_DIR) $(BUILD_DIR) 2>/dev/null || true

distclean: clean
	@echo "Performing deep clean..."
	@rm -f *.log *.tmp *.core 2>/dev/null || true

install: $(TARGET)
	@echo "Installing $(TARGET) to $(BINDIR)"
	@mkdir -p $(BINDIR)
	@cp -f $(BUILD_DIR)/$(TARGET) $(BINDIR)/
	@chmod 755 $(BINDIR)/$(TARGET)
	@echo "Installation complete."

uninstall:
	@echo "Uninstalling $(TARGET) from $(BINDIR)"
	@rm -f $(BINDIR)/$(TARGET)
	@echo "Uninstallation complete."

info:
	@echo "=== Project Information ==="
	@echo "Project: $(PROJECT_NAME) v$(VERSION)"
	@echo "Platform: $(PLATFORM)-$(ARCH)"
	@echo "C++ Compiler: $(CXX) ($(COMPILER_TYPE))"
	@echo "C Compiler: $(CC)"
	@echo "Build Type: $(BUILD_TYPE)"
	@echo "Detected pkg-config: $(HAS_PKG_CONFIG)"
	@echo ""
	@echo "=== Library Information ==="
	@echo "Requested: $(LIBRARY_NEED)"
	@echo "Manual: $(MANUAL_LIBS)"
	@echo "Detected Cflags: $(LIB_CFLAGS)"
	@echo "Detected LDFLAGS: $(LIB_LDFLAGS)"
	@echo ""
	@echo "=== Path and File Info ==="
	@echo "Source files (.cpp): $(wordlist 1,5,$(SOURCES_CPP))$(if $(filter-out $(wordlist 1,5,$(SOURCES_CPP)),$(SOURCES_CPP)), ...)"
	@echo "Source files (.c):   $(wordlist 1,5,$(SOURCES_C))$(if $(filter-out $(wordlist 1,5,$(SOURCES_C)),$(SOURCES_C)), ...)"
	@echo "Install Prefix: $(PREFIX)"

format:
	@command -v clang-format >/dev/null 2>&1 && \
		echo "Formatting code..." && \
		find $(SRC_DIR) $(INC_DIR) $(LIB_DIR) $(EXTERNAL_LIB_DIR) -name "*.cpp" -o -name "*.h" -o -name "*.c" | \
		xargs clang-format -i || \
		echo "clang-format not found, skipping formatting."

strip_target:
	@if [ "$(BUILD_TYPE)" != "debug" ]; then \
		if command -v strip >/dev/null 2>&1; then \
			echo "Stripping symbols..."; \
			strip $(BUILD_DIR)/$(TARGET); \
		else \
			echo "strip not found, skipping stripping."; \
		fi; \
	fi
