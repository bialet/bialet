TARGET_EXEC := bialet
BUILD_DIR := ./build
SRC_DIRS := ./src
DOCS_DIRS := ./docs
TEST_DIR := ./tests
INSTALL_DIR := ~/.local/bin
DB_FILE := _db.sqlite3
OS := $(shell uname -s)

SPHINXBUILD ?= sphinx-build
SPHINXOPTS ?=

SRCS := $(shell find $(SRC_DIRS) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
OBJ_DIRS := $(sort $(dir $(OBJS)))

WREN_FILES := $(shell find $(SRC_DIRS) -name '*.wren')

CFLAGS := -Wall -g
LDFLAGS := -std=c17 -lm -lpthread -lsqlite3 -lcurl

# Not checking against OS because I compile with Wine on Linux
ifneq (,$(findstring x86_64-w64-mingw32-gcc,$(CC)))
    # If it does, append -lws2_32 to LDFLAGS
    LDFLAGS += -lws2_32
endif

ifeq (,$(findstring Darwin,$(OS)))
HAVE_SSL := $(shell echo "#include <openssl/ssl.h>" | $(CC) -E - 2>/dev/null && echo 1 || echo 0)
ifeq ($(HAVE_SSL),1)
	CFLAGS += -DHAVE_SSL
	LDFLAGS += -lssl -lcrypto
endif
else
		CFLAGS += $(shell pkg-config --cflags openssl) -DHAVE_SSL
		LDFLAGS += $(shell pkg-config --libs openssl)
endif

all: $(BUILD_DIR)/$(TARGET_EXEC)

wren_to_c_string:
	@for file in $(WREN_FILES); do \
		python3 tools/wren_to_c_string.py $$file.inc $$file; \
	done

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c | $(OBJ_DIRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIRS):
	@mkdir -p $@

installcheck: install
	$(TEST_DIR)/run.sh $(INSTALL_DIR)/$(TARGET_EXEC)

check: $(BUILD_DIR)/$(TARGET_EXEC)
	$(TEST_DIR)/run.sh

install: $(BUILD_DIR)/$(TARGET_EXEC)
	mkdir -p $(INSTALL_DIR)
	cp $(BUILD_DIR)/$(TARGET_EXEC) $(INSTALL_DIR)

uninstall:
	rm -f $(INSTALL_DIR)/$(TARGET_EXEC)

clean:
	rm -rf $(BUILD_DIR)
	find . -name "$(DB_FILE)*" -type f -delete
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"


html:
	@$(SPHINXBUILD) -M html "$(DOCS_DIRS)" "$(BUILD_DIR)" $(SPHINXOPTS) $(O)

.PHONY: all clean wren_to_c_string install uninstall check html
