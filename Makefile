CC = gcc 
CCFLAGS = -O3 -I$(SOURCE_DIR)  
LDFLAGS =
DEBUGFLAGS = -g -O0
SANFLAGS = -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined
BUILD_DIR = build
SOURCE_DIR = src

SOURCES := $(shell find $(SOURCE_DIR) -name "*.c")
OBJECTS = $(SOURCES:$(SOURCE_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPENDENCIES = $(SOURCES:.c=.d)
TARGET = $(BUILD_DIR)/main

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CCFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c Makefile 
	mkdir -p $(dir $@)
	$(CC) $(CCFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDENCIES)


debug: CCFLAGS += $(DEBUGFLAGS)
debug: $(TARGET)

sanitize: CCFLAGS += $(SANFLAGS)
sanitize: LDFLAGS += -fsanitize=address,undefined
sanitize: clean $(TARGET)

run:
	./$(TARGET) ./tests/prova4.txt

clean:
	rm -rf $(BUILD_DIR)

.PHONY: run clean debug sanitize