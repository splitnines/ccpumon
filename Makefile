# ===============================
# Makefile for csshnet project
# ===============================

# --- Compiler and flags ---
CC          := gcc
INCLUDE_DIR := include
CFLAGS      := -Wall -Wextra -O2 -I$(INCLUDE_DIR) -MMD -MP
DBGFLAGS    := -Wall -Wextra -g -O0 -I$(INCLUDE_DIR) -MMD -MP
LDFLAGS     := -lssh -lpthread

# --- Directories ---
SRC_DIR     := src
OBJ_DIR     := obj
BLD_DIR     := build
DBG_DIR     := build_debug
TEST_DIR    := tests

# --- Targets ---
TARGET      := ccpumon
TEST_TARGET := test_ccpumon

# --- Source/object/dependency files ---
SRCS        := $(wildcard $(SRC_DIR)/*.c)
OBJS        := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS        := $(OBJS:.o=.d)

# Coverage objects
COV_OBJ_DIR := obj_coverage
COV_BLD_DIR := build_coverage
COV_OBJS    := $(SRCS:$(SRC_DIR)/%.c=$(COV_OBJ_DIR)/%.o)
COV_DEPS    := $(COV_OBJS:.o=.d)

# ===============================
# Default target
# ===============================
all: $(BLD_DIR)/$(TARGET)

# ===============================
# Build rules
# ===============================

# --- Release build ---
$(BLD_DIR)/$(TARGET): $(OBJS) | $(BLD_DIR)
	@echo "Linking release build -> $@"
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(COV_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(COV_OBJ_DIR)
	@echo "Compiling (coverage) $< -> $@"
	$(CC) -Wall -Wextra -g -O0 -I$(INCLUDE_DIR) -MMD -MP --coverage -c $< -o $@

# --- Debug build ---
debug: CFLAGS := $(DBGFLAGS)
debug: OBJ_DIR := obj_debug
debug: BLD_DIR := $(DBG_DIR)
debug: OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
debug: DEPS := $(OBJS:.o=.d)
debug: $(BLD_DIR)/$(TARGET)

coverage: $(COV_BLD_DIR)/$(TARGET)

$(COV_BLD_DIR)/$(TARGET): $(COV_OBJS) | $(COV_BLD_DIR)
	@echo "Linking coverage build -> $@"
	$(CC) -Wall -Wextra -g -O0 -I$(INCLUDE_DIR) -MMD -MP --coverage -o $@ $(COV_OBJS) -lssh -lpthread --coverage

# --- Coverage build (gcov) ---
coverage: CFLAGS := -Wall -Wextra -g -O0 -I$(INCLUDE_DIR) -MMD -MP --coverage
coverage: LDFLAGS := -lssh -lpthread --coverage
coverage: OBJ_DIR := obj_coverage
coverage: BLD_DIR := build_coverage
coverage: OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
coverage: DEPS := $(OBJS:.o=.d)
coverage: $(BLD_DIR)/$(TARGET)

# --- Compile source to object ---
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "Compiling $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

# ===============================
# Unit tests
# ===============================

# --- Exclude main.o when linking tests ---
TEST_OBJS := $(filter-out $(OBJ_DIR)/main.o,$(OBJS))
TEST_BIN  := $(BLD_DIR)/$(TEST_TARGET)

# --- Build + run tests ---
tests: $(TEST_BIN)
	@echo "\nRunning tests..."
	@$(TEST_BIN)

$(TEST_BIN): $(TEST_DIR)/test_csshnet.c $(TEST_OBJS) | $(BLD_DIR)
	@echo "Linking test binary -> $@"
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ===============================
# Utility targets
# ===============================

# --- Create directories if missing ---
$(OBJ_DIR) $(BLD_DIR) $(DBG_DIR) $(COV_OBJ_DIR) $(COV_BLD_DIR):
	mkdir -p $@

# --- Clean build artifacts ---
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(OBJ_DIR) $(BLD_DIR) obj_debug $(DBG_DIR) $(COV_OBJ_DIR) $(COV_BLD_DIR) *.gcov

# --- Include auto-generated dependencies ---
-include $(DEPS)

# ===============================
# Install target into ~/.local/bin
# ===============================

PREFIX      := $(HOME)/.local
BINDIR      := $(PREFIX)/bin

install: $(BLD_DIR)/$(TARGET)
	@echo "Installing $(TARGET) to $(BINDIR)"
	mkdir -p $(BINDIR)
	rm $(BINDIR)/$(TARGET)
	cp $(BLD_DIR)/$(TARGET) $(BINDIR)/$(TARGET)

.PHONY: all clean debug tests

