# Filesystem 模組重點程式碼說明

本文件說明 YunFS 檔案系統模組的核心實作與設計重點。

## 目錄結構

```
src/filesystem/
├── vfs.h / vfs.c          # 虛擬檔案系統核心
├── fileops.h / fileops.c  # 實體檔案操作封裝
├── path.h / path.c        # 路徑處理工具
└── vfs_persist.h / vfs_persist.c  # VFS 持久化與加密
```

---

## 1. 虛擬檔案系統核心 (VFS)

### 1.1 核心資料結構

#### VFS 節點結構 (`vfs_node_t`)

```c
typedef struct vfs_node {
    char *name;                    // 節點名稱
    vfs_node_type_t type;          // 節點類型（檔案/目錄）
    void *data;                    // 檔案內容（目錄為 NULL）
    size_t size;                   // 大小（檔案：位元組數，目錄：子節點數）
    time_t mtime;                  // 最後修改時間
    time_t ctime;                  // 建立時間
    struct vfs_node *parent;       // 父節點指標
    struct vfs_node *children;     // 第一個子節點（鏈結串列頭）
    struct vfs_node *next;         // 下一個兄弟節點
} vfs_node_t;
```

**設計重點：**
- 使用樹狀結構管理檔案與目錄
- 透過 `children` 與 `next` 指標實作鏈結串列，管理同層級節點
- 每個節點記錄父節點，支援向上遍歷

#### VFS 系統結構 (`vfs_t`)

```c
typedef struct {
    vfs_node_t *root;      // 根目錄節點
    size_t total_nodes;    // 總節點數
    size_t total_size;     // 總大小（位元組）
} vfs_t;
```

**設計重點：**
- 維護根節點作為整個檔案系統的起點
- 追蹤總節點數與總大小，方便統計與監控

### 1.2 路徑解析機制

#### `resolve_path()` - 路徑解析核心函式

```c
static vfs_node_t *resolve_path(vfs_t *vfs, const char *path, bool create_dirs)
```

**實作邏輯：**

1. **路徑規範化**
   ```c
   char *normalized = normalize_path(path);
   ```
   - 處理相對路徑、`..`、`.` 等特殊路徑
   - 統一路徑格式

2. **路徑分割**
   ```c
   char **components = split_path(normalized, &count);
   ```
   - 將路徑字串分割為目錄組件陣列
   - 例如：`/home/user/file.txt` → `["home", "user", "file.txt"]`

3. **節點遍歷**
   ```c
   vfs_node_t *current = vfs->root;
   for (size_t i = 0; i < count; i++) {
       vfs_node_t *child = find_child(current, components[i]);
       if (child == NULL) {
           if (create_dirs && i < count - 1) {
               // 自動建立中間目錄
               child = create_node(components[i], VFS_DIR);
               add_child(current, child);
           }
       }
       current = child;
   }
   ```
   - 從根節點開始，依序尋找每個路徑組件
   - 若 `create_dirs` 為 true，自動建立不存在的中間目錄
   - 回傳最終節點指標

**設計重點：**
- 支援自動建立中間目錄，簡化檔案建立流程
- 統一路徑解析邏輯，避免重複實作

### 1.3 檔案建立流程

#### `vfs_create_file()` - 建立檔案

```c
vfs_node_t *vfs_create_file(vfs_t *vfs, const char *path, const void *data, size_t size)
```

**實作步驟：**

1. **安全性檢查**
   ```c
   if (is_path_traversal(path)) {
       return NULL;  // 防止路徑遍歷攻擊
   }
   ```

2. **路徑分解**
   ```c
   char *dir_path = path_get_dirname(path);   // 取得目錄部分
   char *filename = path_get_basename(path);  // 取得檔案名稱
   ```

3. **父目錄解析**
   ```c
   vfs_node_t *parent = resolve_path(vfs, dir_path, true);
   ```
   - 自動建立不存在的父目錄

4. **檢查檔案是否已存在**
   ```c
   vfs_node_t *existing = find_child(parent, filename);
   if (existing != NULL) {
       return NULL;  // 檔案已存在
   }
   ```

5. **建立檔案節點並複製資料**
   ```c
   vfs_node_t *file = create_node(filename, VFS_FILE);
   file->data = safe_malloc(size);
   memcpy(file->data, data, size);
   file->size = size;
   ```

6. **加入父目錄**
   ```c
   add_child(parent, file);
   ```

**設計重點：**
- 完整的錯誤處理與安全性檢查
- 自動建立父目錄，提升使用便利性
- 使用安全記憶體管理函式（`safe_malloc`）

### 1.4 節點管理機制

#### `add_child()` - 新增子節點

```c
static bool add_child(vfs_node_t *parent, vfs_node_t *child)
```

**實作邏輯：**
- 將新節點加入父節點的 `children` 鏈結串列
- 設定子節點的 `parent` 指標
- 更新父節點的 `size`（目錄大小 = 子節點數）

#### `remove_child()` - 移除子節點

```c
static bool remove_child(vfs_node_t *parent, vfs_node_t *child)
```

**實作邏輯：**
- 從父節點的 `children` 鏈結串列中移除節點
- 處理鏈結串列的頭節點與中間節點兩種情況
- 更新父節點的 `size`

#### `destroy_node()` - 遞迴銷毀節點

```c
static void destroy_node(vfs_node_t *node)
```

**實作邏輯：**
```c
// 遞迴銷毀所有子節點
vfs_node_t *child = node->children;
while (child != NULL) {
    vfs_node_t *next = child->next;
    destroy_node(child);
    child = next;
}

// 安全清除並釋放檔案資料
if (node->type == VFS_FILE && node->data != NULL) {
    secure_zero(node->data, node->size);  // 安全清除敏感資料
    safe_free(node->data);
}
```

**設計重點：**
- 遞迴銷毀確保所有子節點都被正確釋放
- 使用 `secure_zero()` 清除敏感資料，防止記憶體洩漏

---

## 2. 實體檔案操作 (FileOps)

### 2.1 安全性設計

#### `fileops_open()` - 安全開啟檔案

```c
FILE *fileops_open(const char *path, const char *mode)
```

**安全性檢查流程：**

1. **路徑遍歷攻擊檢查**
   ```c
   if (is_path_traversal(path)) {
       return NULL;  // 防止 ../ 等路徑攻擊
   }
   ```

2. **路徑規範化**
   ```c
   char *normalized = normalize_path(path);
   ```

3. **路徑長度驗證**
   ```c
   if (!validate_path_length(normalized, 4096)) {
       return NULL;  // 限制路徑長度
   }
   ```

4. **開啟檔案**
   ```c
   FILE *file = fopen(normalized, mode);
   ```

**設計重點：**
- 所有檔案操作前都進行路徑驗證
- 統一使用規範化路徑，避免路徑不一致問題
- 限制路徑長度，防止緩衝區溢位

### 2.2 檔案讀寫操作

#### `fileops_read()` - 讀取檔案

**實作重點：**
- 使用 `fileops_open()` 安全開啟檔案
- 動態配置記憶體緩衝區
- 限制檔案大小（100MB），防止記憶體耗盡
- 使用 `safe_malloc()` 進行安全記憶體配置

#### `fileops_write()` - 寫入檔案

**實作重點：**
- 使用 `fileops_open()` 安全開啟檔案
- 原子性寫入（先寫入暫存檔，再移動）
- 完整的錯誤處理

---

## 3. 路徑處理工具 (Path)

### 3.1 路徑分解函式

#### `path_get_dirname()` - 取得目錄部分

```c
char *path_get_dirname(const char *path)
```

**範例：**
- `/home/user/file.txt` → `/home/user`
- `file.txt` → `.`
- `/` → `/`

#### `path_get_basename()` - 取得檔案名稱

```c
char *path_get_basename(const char *path)
```

**範例：**
- `/home/user/file.txt` → `file.txt`
- `/home/user/` → `user`
- `file.txt` → `file.txt`

### 3.2 路徑驗證函式

#### `path_is_absolute()` - 判斷是否為絕對路徑

```c
bool path_is_absolute(const char *path)
```

**實作：**
```c
return (path != NULL && path[0] == '/');
```

#### `path_get_extension()` - 取得副檔名

```c
const char *path_get_extension(const char *path)
```

**實作邏輯：**
- 尋找最後一個 `.` 字元
- 回傳副檔名字串（不含 `.`）

---

## 4. VFS 持久化與加密 (VFS Persist)

### 4.1 序列化機制

#### `serialize_node()` - 節點序列化

**序列化格式：**
1. 節點類型（1 byte）
2. 名稱長度（4 bytes）
3. 名稱字串
4. 檔案大小（8 bytes，僅檔案）
5. 檔案資料（僅檔案）
6. 子節點數量（4 bytes）
7. 遞迴序列化所有子節點

**設計重點：**
- 使用前序遍歷（深度優先）序列化樹狀結構
- 固定大小欄位使用固定長度編碼，提升效率

#### `deserialize_node()` - 節點反序列化

**實作邏輯：**
1. 讀取節點類型與名稱
2. 建立節點
3. 讀取檔案資料（若為檔案）
4. 讀取子節點數量
5. 遞迴反序列化所有子節點
6. 建立父子關係

### 4.2 加密儲存

#### `vfs_save_encrypted()` - 加密儲存 VFS

**實作流程：**

1. **計算序列化大小**
   ```c
   size_t serialized_size = sizeof(VFS_MAGIC) + sizeof(uint32_t) + 
                            calculate_serialized_size(vfs->root);
   ```

2. **序列化 VFS**
   ```c
   // 寫入魔數（識別檔案格式）
   memcpy(buffer + offset, VFS_MAGIC, sizeof(VFS_MAGIC) - 1);
   
   // 寫入版本號
   *(uint32_t *)(buffer + offset) = VFS_VERSION;
   
   // 序列化根節點
   offset = serialize_node(vfs->root, buffer, serialized_size, offset);
   ```

3. **ChaCha20 加密**
   ```c
   uint8_t nonce[12] = {0};
   memcpy(nonce, "yunhongisbest", 12);
   chacha20_encrypt_with_key(key, nonce, buffer, encrypted, offset);
   ```

4. **寫入檔案**
   ```c
   fwrite(&offset, sizeof(size_t), 1, file);  // 寫入資料大小
   fwrite(encrypted, 1, offset, file);        // 寫入加密資料
   ```

5. **安全清除**
   ```c
   secure_zero(buffer, serialized_size);   // 清除明文
   secure_zero(encrypted, offset);         // 清除加密資料
   ```

**設計重點：**
- 使用 ChaCha20 串流加密，適合大檔案
- 儲存前清除明文緩衝區，防止記憶體洩漏
- 包含魔數與版本號，方便格式驗證

#### `vfs_load_encrypted()` - 解密載入 VFS

**實作流程：**

1. **讀取加密檔案**
   ```c
   fread(&data_size, sizeof(size_t), 1, file);
   fread(encrypted, 1, data_size, file);
   ```

2. **ChaCha20 解密**
   ```c
   chacha20_decrypt_with_key(key, nonce, encrypted, decrypted, data_size);
   ```

3. **驗證魔數與版本**
   ```c
   if (memcmp(decrypted, VFS_MAGIC, sizeof(VFS_MAGIC) - 1) != 0) {
       return NULL;  // 格式錯誤
   }
   ```

4. **反序列化 VFS**
   ```c
   vfs_t *vfs = vfs_init();
   vfs->root = deserialize_node(decrypted, data_size, &offset, NULL);
   ```

**設計重點：**
- 完整的格式驗證，防止錯誤檔案
- 若檔案不存在，回傳新建立的空 VFS
- 密鑰錯誤時回傳 NULL，不洩漏錯誤原因（安全性考量）

---

## 5. 安全性設計重點

### 5.1 路徑驗證

所有路徑操作都經過以下檢查：
- **路徑遍歷攻擊檢查**：防止 `../` 等攻擊
- **路徑長度限制**：防止緩衝區溢位
- **路徑規範化**：統一處理相對路徑與特殊字元

### 5.2 記憶體安全

- 使用 `safe_malloc()` / `safe_free()` 進行記憶體管理
- 敏感資料使用 `secure_zero()` 清除
- 所有字串操作都檢查 NULL 指標

### 5.3 錯誤處理

- 統一的錯誤碼系統（`error.h`）
- 所有函式都有完整的錯誤檢查
- 失敗時正確釋放已配置的資源

---

## 6. 模組間依賴關係

```
vfs.c
  ├── path.h          (路徑處理)
  ├── sanitize.h      (路徑規範化)
  ├── validation.h    (路徑驗證)
  ├── memory.h        (安全記憶體管理)
  └── error.h         (錯誤處理)

fileops.c
  ├── sanitize.h      (路徑規範化)
  ├── validation.h    (路徑驗證)
  ├── memory.h        (安全記憶體管理)
  └── error.h         (錯誤處理)

vfs_persist.c
  ├── vfs.h           (VFS 核心)
  ├── chacha20.h      (加密演算法)
  ├── memory.h        (安全記憶體管理)
  └── error.h         (錯誤處理)
```

---

## 7. 使用範例

### 建立檔案系統並建立檔案

```c
// 初始化 VFS
vfs_t *vfs = vfs_init();

// 建立檔案
const char *data = "Hello, World!";
vfs_node_t *file = vfs_create_file(vfs, "/home/user/hello.txt", 
                                    data, strlen(data));

// 讀取檔案
size_t size;
void *content = vfs_read_file(file, &size);

// 加密儲存
vfs_save_encrypted(vfs, "filesystem.dat", "my_secret_key");

// 載入 VFS
vfs_t *loaded = vfs_load_encrypted("filesystem.dat", "my_secret_key");

// 清理
vfs_destroy(vfs);
vfs_destroy(loaded);
```

### 實體檔案操作

```c
// 安全開啟檔案
FILE *file = fileops_open("/path/to/file.txt", "rb");

// 讀取檔案內容
void *data;
size_t size;
if (fileops_read("/path/to/file.txt", &data, &size)) {
    // 處理資料
    safe_free(data);
}

// 寫入檔案
const char *content = "Hello";
fileops_write("/path/to/output.txt", content, strlen(content));
```

---

## 總結

Filesystem 模組的核心設計理念：

1. **安全性優先**：所有路徑操作都經過驗證，防止常見攻擊
2. **記憶體安全**：使用安全記憶體管理函式，防止記憶體洩漏
3. **模組化設計**：清晰的模組劃分，易於維護與擴展
4. **完整錯誤處理**：統一的錯誤處理機制，提升穩定性
5. **加密保護**：使用 ChaCha20 加密保護持久化資料

