# Vim 功能實現計劃

## 已實現的基礎功能
- 基本移動：h, j, k, l
- 基本插入：i, a
- 基本刪除：x
- 命令模式：:w, :q, :wq, :e, :b

## 需要實現的完整功能

### Normal 模式
1. **移動操作**
   - w, b, e, W, B, E (單詞移動)
   - 0, ^, $ (行首行尾)
   - gg, G, {number}G (跳轉到行)
   - H, M, L (屏幕位置)
   - {, } (段落移動)
   - f{char}, F{char}, t{char}, T{char} (查找字元)
   - %, * (匹配括號/單詞)

2. **編輯操作**
   - dd, dw, de, d$, d^, D (刪除)
   - yy, yw, ye, y$, y^, Y (複製)
   - p, P (粘貼)
   - x, X (刪除字元)
   - r{char}, R (替換)
   - s, S, cw, ce, c$, c^, C, cc (修改)
   - u, Ctrl+r (撤銷/重做)
   - . (重複上一個操作)
   - J (合併行)
   - >>, << (縮進)

3. **插入操作**
   - i, I, a, A, o, O (插入模式)

4. **搜索**
   - /{pattern}, ?{pattern} (搜索)
   - n, N (下一個/上一個匹配)

### Insert 模式
- Ctrl+w (刪除單詞)
- Ctrl+u (刪除到行首)
- Ctrl+h (等同 Backspace)

### Visual 模式
- v (字符選擇)
- V (行選擇)
- 選擇後的編輯操作 (d, y, c, etc.)

### Command 模式
- :s/old/new/g (替換)
- :set (設置選項)
- :nohl (取消高亮)

### 其他
- 撤銷/重做系統
- 寄存器系統 (a-z)
- 搜索高亮



