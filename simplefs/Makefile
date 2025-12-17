CC       := gcc
CFLAGS   := -Wall -Wextra -g -O2 -D_FILE_OFFSET_BITS=64
CFLAGS  += $(shell pkg-config fuse3 --cflags)
CFLAGS  += -DFUSE_USE_VERSION=31
LDFLAGS += $(shell pkg-config fuse3 --libs)

TARGET   := simplefs

SRC_DIR  := src
OBJ_DIR  := obj
BIN_DIR  := bin


SRCS     := $(shell find $(SRC_DIR) -name "*.c")
OBJS     := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
DEPS     := $(OBJS:.o=.d)

FORMATTER := clang-format
FORMAT_STYLE := Google
FORMAT_SOURCES := $(shell find $(SRC_DIR) -name "*.c" -o -name "*.h")

all: $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

-include $(DEPS)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

format:
	@echo "Formatting source files..."
	$(FORMATTER) -i -style=$(FORMAT_STYLE) $(FORMAT_SOURCES)
	@echo "Formatting complete."

.PHONY: all clean format