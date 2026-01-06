# Security 模組架構文件

## 概述

`security` 資料夾包含三個核心安全模組，提供檔案系統的加密、路徑安全處理與輸入驗證功能。這些模組共同構成了 YunFS 的安全防護層。

## 模組架構

```
security/
├── chacha20.c/h      # ChaCha20 串流加密模組
├── sanitize.c/h      # 路徑清理與規範化模組
└── validation.c/h    # 輸入驗證模組
```

---

## 1. ChaCha20 加密模組

### 1.1 模組職責

提供對稱串流加密功能，用於保護檔案系統中的敏感資料。ChaCha20 是一種高效能的加密演算法，符合 RFC 7539 標準。

### 1.2 核心功能

#### 加密初始化
- **函式**: `chacha20_init()`
- **功能**: 設定加密所需的密鑰、nonce 與計數器
- **參數**:
  - `key`: 32 位元組密鑰
  - `nonce`: 12 位元組 nonce（一次性數值）
  - `counter`: 初始計數器值（通常為 0）

#### 加密/解密操作
- **函式**: `chacha20_encrypt()`
- **功能**: 對稱加密/解密（使用 XOR 操作）
- **特性**: 加密與解密使用相同函式，對已加密資料再次呼叫即可解密

#### 密鑰衍生
- **函式**: `chacha20_derive_key()`
- **功能**: 將任意長度的密鑰字串擴展為標準的 32 位元組密鑰
- **注意**: 此為簡化實作，正式環境建議使用標準 KDF

#### 便利函式
- **函式**: `chacha20_encrypt_with_key()`
- **功能**: 整合密鑰衍生與加密操作的便利函式

### 1.3 演算法實作細節

#### ChaCha20 狀態結構
- 16 個 32 位元字（共 512 位元）
- 狀態組成：
  - 位置 0-3: 初始化常數 "expand 32-byte k"
  - 位置 4-11: 256 位元密鑰（8 個 32 位元字）
  - 位置 12: 計數器
  - 位置 13-15: 96 位元 nonce（3 個 32 位元字）

#### 核心運算
- **四分之一輪函式** (`quarter_round`): 執行核心混合操作
- **區塊產生** (`chacha20_block`): 執行 20 輪（10 次雙輪）混合操作，產生 64 位元組的密鑰流
- **加密流程**: 產生密鑰流後與輸入資料進行 XOR 運算

### 1.4 使用範例

```c
// 初始化加密
uint8_t key[32] = {...};  // 32 位元組密鑰
uint8_t nonce[12] = {...}; // 12 位元組 nonce
chacha20_init(key, nonce, 0);

// 加密資料
uint8_t plaintext[] = "Hello, World!";
uint8_t ciphertext[sizeof(plaintext)];
chacha20_encrypt(plaintext, ciphertext, sizeof(plaintext));

// 解密（使用相同函式）
uint8_t decrypted[sizeof(plaintext)];
chacha20_init(key, nonce, 0);  // 重新初始化
chacha20_encrypt(ciphertext, decrypted, sizeof(ciphertext));
```

---

## 2. Sanitize 路徑清理模組

### 2.1 模組職責

提供路徑字串的安全處理功能，防止路徑遍歷攻擊與處理無效路徑。

### 2.2 核心功能

#### 路徑清理
- **函式**: `sanitize_path()`
- **功能**: 移除路徑中的危險字元
- **允許字元**: 英文字母、數字、斜線、點號、連字號、底線、空格
- **輸出**: 清理後的路徑（需由呼叫者釋放）

#### 路徑規範化
- **函式**: `normalize_path()`
- **功能**: 處理路徑中的冗餘元素
  - 移除重複的斜線
  - 移除結尾的斜線（根目錄除外）
- **安全檢查**: 自動檢測路徑遍歷攻擊

#### 路徑遍歷檢測
- **函式**: `is_path_traversal()`
- **功能**: 偵測常見的路徑遍歷模式
  - `../` 序列
  - 以 `..` 開頭
  - `/../` 序列
- **回傳**: `true` 表示偵測到攻擊

#### 路徑處理
- **函式**: `remove_duplicate_slashes()`
  - 移除路徑中的重複斜線
  
- **函式**: `safe_path_join()`
  - 安全地連接兩個路徑
  - 自動處理斜線
  - 檢查路徑遍歷攻擊與路徑長度限制

### 2.3 安全機制

#### 路徑長度限制
- 最大路徑長度: 4096 位元組（`MAX_PATH_LEN`）
- 所有函式都會驗證路徑長度

#### 路徑遍歷防護
- 檢測 `../` 模式
- 檢測以 `..` 開頭的路徑
- 檢測 `/../` 模式
- 檢測到攻擊時回傳 `NULL` 並設定錯誤訊息

### 2.4 使用範例

```c
// 清理路徑
char *path = sanitize_path("user/../../etc/passwd");
// 結果: 移除危險字元，但 is_path_traversal() 會偵測到攻擊

// 規範化路徑
char *normalized = normalize_path("user//documents//file.txt");
// 結果: "user/documents/file.txt"

// 安全連接路徑
char *joined = safe_path_join("/base/path", "subdir/file.txt");
// 結果: "/base/path/subdir/file.txt"
```

---

## 3. Validation 輸入驗證模組

### 3.1 模組職責

提供各種輸入驗證功能，防止緩衝區溢位、注入攻擊與無效輸入導致的程式錯誤。

### 3.2 核心功能

#### 字串驗證

**字串長度驗證**
- **函式**: `validate_string_length()`
- **功能**: 驗證字串長度是否在安全範圍內
- **實作**: 使用 `strnlen()` 避免在超長字串上花費過多時間
- **參數**:
  - `str`: 要驗證的字串
  - `max_len`: 最大允許長度

**字串字元驗證**
- **函式**: `validate_string_chars()`
- **功能**: 驗證字串是否僅包含有效字元
- **用途**: 防止注入攻擊
- **參數**:
  - `str`: 要驗證的字串
  - `allowed_chars`: 允許的字元集（NULL 表示允許所有可列印字元）

#### 緩衝區驗證

**緩衝區邊界檢查**
- **函式**: `validate_buffer_bounds()`
- **功能**: 檢查指定的偏移量與大小是否在緩衝區範圍內
- **防護**: 防止緩衝區溢位攻擊與整數溢位
- **參數**:
  - `offset`: 存取起始偏移量
  - `size`: 要存取的大小
  - `buffer_size`: 緩衝區總大小

#### 數值驗證

**整數範圍驗證**
- **函式**: `validate_int_range()`
- **功能**: 驗證整數是否在指定範圍內
- **參數**:
  - `value`: 要驗證的整數
  - `min`: 最小值（含）
  - `max`: 最大值（含）

#### 檔案系統驗證

**檔案名稱驗證**
- **函式**: `validate_filename()`
- **功能**: 檢查檔案名稱是否符合安全規範
- **檢查項目**:
  - 長度在 255 字元以內（`MAX_FILENAME_LEN`）
  - 不包含斜線等禁止字元
  - 不以 `..` 開頭
  - 不為空字串

**路徑長度驗證**
- **函式**: `validate_path_length()`
- **功能**: 驗證路徑長度
- **預設限制**: 4096 位元組（`MAX_PATH_LEN`）

### 3.3 安全常數

```c
#define MAX_PATH_LEN 4096      // POSIX 標準最大路徑長度
#define MAX_FILENAME_LEN 255   // 最大檔案名稱長度
```

### 3.4 使用範例

```c
// 驗證字串長度
if (!validate_string_length(user_input, 256)) {
    // 處理錯誤
}

// 驗證緩衝區存取
if (!validate_buffer_bounds(offset, size, buffer_size)) {
    // 處理邊界錯誤
}

// 驗證檔案名稱
if (!validate_filename(filename)) {
    // 處理無效檔案名稱
}
```

---

## 模組間協作

### 依賴關係

```
sanitize.c
  └── validation.h  (使用 validate_path_length)

validation.c
  └── error.h       (錯誤處理)

chacha20.c
  └── memory.h      (記憶體管理)
```

### 典型使用流程

1. **路徑處理流程**:
   ```
   使用者輸入路徑
   → validate_path_length() 驗證長度
   → is_path_traversal() 檢查路徑遍歷
   → sanitize_path() 清理危險字元
   → normalize_path() 規範化路徑
   ```

2. **加密流程**:
   ```
   原始資料
   → chacha20_derive_key() 衍生密鑰
   → chacha20_init() 初始化加密狀態
   → chacha20_encrypt() 加密資料
   ```

3. **輸入驗證流程**:
   ```
   使用者輸入
   → validate_string_length() 檢查長度
   → validate_string_chars() 檢查字元
   → validate_buffer_bounds() 檢查緩衝區邊界
   ```

---

## 安全設計原則

### 1. 深度防禦（Defense in Depth）
- 多層驗證機制
- 每個模組都有獨立的檢查點

### 2. 最小權限原則
- 僅允許必要的字元與操作
- 嚴格限制路徑與檔案名稱格式

### 3. 輸入驗證優先
- 所有外部輸入都必須經過驗證
- 驗證失敗時立即回傳錯誤

### 4. 安全預設值
- 預設拒絕未知輸入
- 明確的白名單機制

### 5. 錯誤處理
- 所有安全檢查失敗時都會設定錯誤訊息
- 使用統一的錯誤處理機制（`error.h`）

---

## 注意事項

### ChaCha20 模組
- ⚠️ **Nonce 重用警告**: 使用相同密鑰時，每次加密必須使用不同的 nonce
- ⚠️ **密鑰衍生**: 目前的 `chacha20_derive_key()` 為簡化實作，正式環境建議使用標準 KDF（如 PBKDF2、Argon2）

### Sanitize 模組
- ⚠️ **記憶體管理**: 所有回傳的字串都需要由呼叫者使用 `safe_free()` 釋放
- ⚠️ **路徑規範化限制**: 目前不處理 `..` 的實際解析，僅檢測攻擊模式

### Validation 模組
- ⚠️ **效能考量**: 字串驗證使用 `strnlen()` 避免在超長字串上花費過多時間
- ⚠️ **字元集驗證**: `validate_string_chars()` 的預設行為允許所有可列印字元，需要時應明確指定允許的字元集

---

## 參考資料

- **ChaCha20**: RFC 7539 - ChaCha20 and Poly1305 for IETF Protocols
- **路徑安全**: OWASP Path Traversal
- **輸入驗證**: CWE-20: Improper Input Validation

