CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
TARGET = yun-fs

# 目錄定義
SRC_DIR = src
BUILD_DIR = build

# 自動搜尋所有 .c 檔案
SOURCES = main.c $(shell find $(SRC_DIR) -name '*.c')

# 將 .o 檔案放到 build/ 目錄
OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

# 自動產生 include 路徑
INCLUDES = -I$(SRC_DIR) $(addprefix -I,$(shell find $(SRC_DIR) -type d))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

# 編譯規則：將 .o 放到 build/ 目錄
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(TARGET) $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

# 顯示偵測到的檔案（除錯用）
info:
	@echo "Sources: $(SOURCES)"
	@echo "Objects: $(OBJECTS)"

.PHONY: all clean run info
