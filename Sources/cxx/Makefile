TARGET := cocodoc

BUILD_DIR := ./build
CSRC_DIR := ../CCocodol/src
INC_DIR := ../CCocodol/include
CSRC := $(shell find $(CSRC_DIR) -type f \( -iname "*.c" ! -iname "main.c" \))
COBJ := $(CSRC:%=%.o)

CFLAGS = -g -Wall -O2
CXXFLAGS = $(shell llvm-config-11 --cxxflags)
LDFLAGS = $(shell llvm-config-11 --ldflags --system-libs --libs core)

cocodoc: $(COBJ)
	$(CXX) -I$(INC_DIR) $(CXXFLAGS) $(addprefix $(BUILD_DIR)/,$(notdir $(COBJ))) main.cc -o $(BUILD_DIR)/$@ $(LDFLAGS)

$(CSRC_DIR)/%.c.o: $(CSRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $(BUILD_DIR)/$(notdir $@)

$(CSRC_DIR)/ast.c:

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)
