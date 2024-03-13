TARGET_EXEC := bialet
BUILD_DIR := ./build
SRC_DIRS := ./src
DOCS_DIRS := ./docs
INSTALL_DIR := ~/.local/bin

SPHINXBUILD ?= sphinx-build
SPHINXOPTS ?=

SRCS := $(shell find $(SRC_DIRS) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

WREN_FILES := $(shell find $(SRC_DIRS) -name '*.wren')

LDFLAGS := -lm -lpthread -lsqlite3 -lssl -lcrypto -lcurl

all: wren_to_c_string $(BUILD_DIR)/$(TARGET_EXEC)

wren_to_c_string:
	@for file in $(WREN_FILES); do \
		python3 tools/wren_to_c_string.py $$file.inc $$file; \
	done

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	mkdir -p $(BUILD_DIR)
	$(CC) -Wall -g $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) -Wall -g -c $< -o $@

install: $(BUILD_DIR)/$(TARGET_EXEC)
	mkdir -p $(INSTALL_DIR)
	cp $(BUILD_DIR)/$(TARGET_EXEC) $(INSTALL_DIR)

uninstall:
	rm -f $(INSTALL_DIR)/$(TARGET_EXEC)

clean:
	rm -rf $(BUILD_DIR)

html:
	@$(SPHINXBUILD) -M html "$(DOCS_DIRS)" "$(BUILD_DIR)" $(SPHINXOPTS) $(O)

.PHONY: all clean wren_to_c_string install uninstall html
