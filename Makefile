TARGET_EXEC := bialet
BUILD_DIR := ./build
SRC_DIRS := ./src

SRCS := $(shell find $(SRC_DIRS) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

WREN_FILES := $(shell find $(SRC_DIRS) -name '*.wren')

LDFLAGS := -lm -lpthread -lsqlite3

all: wren_to_c_string $(BUILD_DIR)/$(TARGET_EXEC)

wren_to_c_string:
	@for file in $(WREN_FILES); do \
		python3 util/wren_to_c_string.py $$file.inc $$file; \
	done

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	mkdir -p $(BUILD_DIR)
	$(CC) -Wall -g $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) -Wall -g -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean wren_to_c_string
