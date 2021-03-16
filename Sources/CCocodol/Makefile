TARGET := cocodol

BUILD_DIR := ./build
SRC_DIR := ./src
INC_DIR := ./include
SRC := $(shell find $(SRC_DIR) -name *.c)
OBJ := $(SRC:%=build/%.o)
DEP := $(OBJ:.o=.d)

CFLAGS = -g -Wall -O2

$(BUILD_DIR)/$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I $(INC_DIR) -c $< -o $@

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)
