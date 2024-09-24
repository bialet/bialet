TARGET_EXEC := bialet
BUILD_DIR := ./build
SRC_DIRS := ./src
DOCS_DIRS := ./docs
INSTALL_DIR := ~/.local/bin
DB_FILE := _db.sqlite3

SPHINXBUILD ?= sphinx-build
SPHINXOPTS ?=

SRCS := $(shell find $(SRC_DIRS) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
OBJ_DIRS := $(sort $(dir $(OBJS)))

WREN_FILES := $(shell find $(SRC_DIRS) -name '*.wren')

LDFLAGS := -std=c17 -lm -lpthread -lsqlite3 -lssl -lcrypto -lcurl

ifneq (,$(findstring x86_64-w64-mingw32-gcc,$(CC)))
    # If it does, append -lws2_32 to LDFLAGS
    LDFLAGS += -lws2_32
endif

all: wren_to_c_string $(BUILD_DIR)/$(TARGET_EXEC)

wren_to_c_string:
	@for file in $(WREN_FILES); do \
		python3 tools/wren_to_c_string.py $$file.inc $$file; \
	done

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) -Wall -g $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c | $(OBJ_DIRS)
	$(CC) -Wall -g -c $< -o $@

$(OBJ_DIRS):
	@mkdir -p $@

install: $(BUILD_DIR)/$(TARGET_EXEC)
	mkdir -p $(INSTALL_DIR)
	cp $(BUILD_DIR)/$(TARGET_EXEC) $(INSTALL_DIR)

uninstall:
	rm -f $(INSTALL_DIR)/$(TARGET_EXEC)

clean:
	rm -rf $(BUILD_DIR)
	find . -name "$(DB_FILE)" -type f -delete

html:
	@$(SPHINXBUILD) -M html "$(DOCS_DIRS)" "$(BUILD_DIR)" $(SPHINXOPTS) $(O)

.PHONY: all clean wren_to_c_string install uninstall html
