# Yun File System

一個專業級的文件系統 CLI 工具，包含虛擬文件系統和類似 vim 的文本編輯器功能。採用模組化架構，注重安全性和代碼質量。文件系統使用 ChaCha20 加密存檔。

## 項目結構

項目採用模組化設計，代碼組織如下：

```
├── main.c                    # 主程序入口
├── Makefile                  # 構建配置
├── README.md                 # 說明文檔
│
└── src/                      # 核心源代碼目錄
    ├── core/                 # 核心功能模組
    │   ├── buffer.c/h        # 文本緩衝區管理（多文件編輯）
    │   ├── editor.c/h        # 編輯器核心（模式切換、命令處理）
    │   ├── command.c/h       # 命令解析和執行
    │   └── shell.c/h         # Shell 介面（ls, mkdir, cd 等命令）
    │
    ├── filesystem/           # 文件系統模組
    │   ├── vfs.c/h           # 虛擬文件系統（記憶體中）
    │   ├── vfs_persist.c/h   # VFS 持久化（ChaCha20 加密）
    │   ├── fileops.c/h       # 實際文件操作（安全封裝）
    │   └── path.c/h          # 路徑處理和驗證
    │
    ├── ui/                   # 用戶界面模組
    │   ├── startup/          # 啟動畫面相關
    │   │   ├── colors.h      # 顏色定義
    │   │   ├── terminal.c/h # 終端操作
    │   │   ├── title.c/h     # 標題顯示
    │   │   ├── menu.c/h      # 菜單顯示
    │   │   └── status.c/h    # 狀態欄顯示
    │   ├── screen.c/h        # 屏幕渲染和刷新（編輯器）
    │   ├── input.c/h         # 輸入處理（原始模式）
    │   └── status.c/h        # 狀態欄和消息顯示（編輯器內）
    │
    ├── security/             # 安全模組
    │   ├── chacha20.c/h      # ChaCha20 加密/解密
    │   ├── validation.c/h   # 輸入驗證
    │   └── sanitize.c/h       # 路徑清理和規範化
    │
    └── utils/                # 工具模組
        ├── error.c/h         # 錯誤處理
        └── memory.c/h        # 記憶體管理封裝
```

## 編譯

```bash
make
```

## 運行

### Shell 模式（預設）

```bash
# 啟動文件系統 shell
./yun-fs

# 在 shell 中可以使用以下命令：
# ls, cd, pwd, mkdir, touch, cat, echo, rm, mv, cp, clear, help, exit
```

### 編輯器模式

```bash
# 使用編輯器打開檔案
./yun-fs <檔案名稱>

# 例如
./yun-fs test.txt
```

## 功能特性

### Shell 命令

啟動程序後會進入一個類似作業系統的 shell 介面，支持以下命令：

- `ls [目錄]` - 列出目錄內容
- `cd [目錄]` - 切換目錄
- `pwd` - 顯示當前目錄路徑
- `mkdir <目錄>` - 創建目錄
- `touch <檔案>` - 創建檔案
- `cat <檔案>` - 顯示檔案內容
- `echo [文本]` - 輸出文本（支持 `echo text > file` 重定向）
- `rm <路徑>` - 刪除檔案或目錄
- `mv <源> <目標>` - 移動/重命名檔案或目錄
- `cp <源> <目標>` - 複製檔案
- `clear` - 清屏
- `help` - 顯示幫助信息
- `exit` - 退出 shell

### 文件系統加密

- **ChaCha20 加密**: 所有文件系統數據使用 ChaCha20 算法加密存檔
- **自動保存**: 退出 shell 時自動保存文件系統狀態（加密）
- **自動載入**: 啟動時自動從加密檔案載入文件系統
- **密鑰**: 使用固定密鑰 "yunhongisbest" 進行加密/解密
- **數據檔案**: 文件系統數據保存在 `.yunfs_data` 檔案中（加密格式）

### Vim 風格編輯器

類似 vim 的文本編輯器，支持：

#### Normal 模式（預設模式）

- `h`, `j`, `k`, `l`: 移動游標（左、下、上、右）
- `i`: 進入插入模式
- `a`: 在游標後進入插入模式
- `v`: 進入可視模式
- `:`: 進入命令模式
- `x`: 刪除當前字元
- `d`: 刪除當前行

#### Insert 模式

- 直接輸入文本
- `Enter`: 換行
- `Backspace`: 刪除字元
- `Esc`: 返回 Normal 模式

#### Command 模式

支持以下 vim 命令：

- `:w` - 保存檔案
- `:w <檔名>` - 另存為
- `:q` - 退出編輯器（如果有未保存的修改會提示）
- `:q!` - 強制退出（不保存）
- `:wq` - 保存並退出
- `:e <檔名>` - 打開檔案
- `:b <n>` - 切換緩衝區（n 為緩衝區編號）

#### Visual 模式

- 文本選擇（基礎實現）
- `Esc`: 返回 Normal 模式

### 安全特性

1. **輸入驗證**: 所有用戶輸入都經過嚴格驗證
2. **緩衝區安全**: 使用 `strnlen`, `snprintf`, `strncpy` 等安全函數
3. **路徑安全**: 防止路徑遍歷，規範化所有路徑
4. **權限檢查**: 檢查文件權限，避免越權操作
5. **記憶體管理**: 使用後釋放，避免記憶體洩漏
6. **錯誤處理**: 完善的錯誤處理和日誌記錄
7. **ChaCha20 加密**: 文件系統數據使用 ChaCha20 加密存檔

## 使用範例

### Shell 模式

```bash
$ ./yun-fs
Yun File System Shell
輸入 'help' 查看可用命令

/ yun-fs$ mkdir test
/ yun-fs$ cd test
/test yun-fs$ touch file.txt
/test yun-fs$ echo "Hello World" > file.txt
/test yun-fs$ cat file.txt
Hello World
/test yun-fs$ cd ..
/ yun-fs$ ls
test/
/ yun-fs$ exit
```

### 編輯器模式

```bash
# 編輯檔案
./yun-fs test.txt

# 在編輯器中
# 1. 按 i 進入插入模式
# 2. 輸入文本
# 3. 按 Esc 返回 Normal 模式
# 4. 輸入 :wq 保存並退出
```

## 模組說明

### 核心模組

- **buffer.c**: 負責文本緩衝區管理，支持多文件編輯、行號管理、修改標記
- **editor.c**: 編輯器核心，處理模式切換、輸入處理、命令執行
- **command.c**: 命令解析器，解析和執行編輯器命令
- **shell.c**: Shell 介面，實現文件系統命令（ls, mkdir, cd 等）

### 文件系統模組

- **vfs.c**: 虛擬文件系統實現，使用樹狀結構存儲文件和目錄
- **vfs_persist.c**: VFS 持久化實現，支持序列化/反序列化和 ChaCha20 加密
- **fileops.c**: 實際文件操作的安全封裝，包含權限檢查和路徑驗證
- **path.c**: 路徑處理工具，提供路徑解析、規範化等功能

### 安全模組

- **chacha20.c**: ChaCha20 流密碼實現，用於文件系統數據加密/解密
- **validation.c**: 輸入驗證，檢查字串長度、字元有效性、緩衝區邊界
- **sanitize.c**: 路徑清理，防止路徑遍歷攻擊、規範化路徑

### UI 模組

- **screen.c**: 屏幕管理，負責渲染文本、顯示狀態欄、處理游標
- **input.c**: 輸入處理，設置原始模式終端、解析鍵盤輸入
- **startup/**: 啟動畫面相關的 UI 代碼

### 工具模組

- **error.c**: 錯誤處理系統，提供統一的錯誤碼和錯誤消息
- **memory.c**: 記憶體管理封裝，提供安全的記憶體分配和釋放函數

## 代碼質量標準

1. **模組化**: 每個模組職責單一，接口清晰
2. **錯誤處理**: 所有函數都有返回值檢查
3. **文檔**: 所有公共函數都有文檔註釋（繁體中文）
4. **安全**: 所有用戶輸入都經過驗證，防止緩衝區溢出和路徑遍歷攻擊
5. **代碼風格**: 遵循一致的命名規範和代碼風格
6. **加密**: 文件系統數據使用 ChaCha20 加密存檔

## 清理

```bash
make clean
```

## 技術棧

- C11 標準
- POSIX API（用於終端控制和文件操作）
- ChaCha20 流密碼算法
- Make 構建系統
- 可選：Valgrind 用於記憶體檢查

## 開發注意事項

- 所有註釋使用繁體中文
- 使用安全的字串函數（strnlen, snprintf, strncpy）
- 所有路徑操作前必須進行路徑遍歷檢查
- 記憶體分配後必須檢查返回值
- 使用後必須釋放記憶體，避免洩漏
- 文件系統數據使用 ChaCha20 加密存檔，密鑰為 "yunhongisbest"

## 數據檔案

文件系統數據保存在 `.yunfs_data` 檔案中，使用 ChaCha20 加密。該檔案會在以下情況自動更新：

- 退出 shell 時自動保存
- 啟動時自動載入（如果檔案存在）
