# Yun File System - 架構流程圖

本文檔使用 Mermaid 流程圖展示 Yun File System 的架構和執行流程。

## 主程序流程

```mermaid
flowchart TD
    Start([程序啟動]) --> CheckArgs{檢查命令行參數}
    CheckArgs -->|有參數| CheckHelp{是否為 --help/-h?}
    CheckArgs -->|無參數| CreateShell[創建 Shell 實例]
    
    CheckHelp -->|是| ShowHelp[顯示幫助訊息]
    CheckHelp -->|否| CreateEditor[創建編輯器實例]
    
    ShowHelp --> End1([結束])
    
    CreateEditor --> OpenFile{開啟檔案}
    OpenFile -->|成功| RunEditor[執行編輯器主迴圈]
    OpenFile -->|失敗| RetryOpen{重試開啟}
    RetryOpen -->|成功| RunEditor
    RetryOpen -->|失敗| DestroyEditor[銷毀編輯器]
    RunEditor --> DestroyEditor
    DestroyEditor --> End2([結束])
    
    CreateShell --> LoadVFS{載入 VFS<br/>從加密檔案}
    LoadVFS -->|成功| InitShell[初始化 Shell]
    LoadVFS -->|失敗| CreateNewVFS[創建新 VFS]
    CreateNewVFS --> InitShell
    InitShell --> RegisterSignal[註冊信號處理器<br/>SIGINT/SIGTERM]
    RegisterSignal --> RunShell[執行 Shell 主迴圈]
    RunShell --> SaveVFS[保存 VFS 到加密檔案]
    SaveVFS --> DestroyShell[銷毀 Shell]
    DestroyShell --> End3([結束])
    
    style Start fill:#90EE90
    style End1 fill:#FFB6C1
    style End2 fill:#FFB6C1
    style End3 fill:#FFB6C1
    style CreateShell fill:#87CEEB
    style CreateEditor fill:#87CEEB
    style RunShell fill:#DDA0DD
    style RunEditor fill:#DDA0DD
```

## Shell 主迴圈流程

```mermaid
flowchart TD
    Start([Shell 啟動]) --> ShowPrompt[顯示命令提示符]
    ShowPrompt --> ReadInput[讀取使用者輸入]
    ReadInput --> CheckEmpty{輸入是否為空?}
    CheckEmpty -->|是| ShowPrompt
    CheckEmpty -->|否| ParseCommand[解析命令和參數]
    
    ParseCommand --> AddHistory[加入命令歷史]
    AddHistory --> FindCommand{查找命令處理函數}
    
    FindCommand -->|找到| ExecuteCommand[執行命令處理函數]
    FindCommand -->|未找到| ShowError[顯示錯誤訊息]
    
    ExecuteCommand --> CheckExit{是否為 exit 命令?}
    ShowError --> ShowPrompt
    
    CheckExit -->|是| SaveVFS[保存 VFS 到加密檔案]
    CheckExit -->|否| ShowPrompt
    
    SaveVFS --> End([結束])
    
    style Start fill:#90EE90
    style End fill:#FFB6C1
    style ExecuteCommand fill:#DDA0DD
    style SaveVFS fill:#87CEEB
```

## Shell 命令執行流程

```mermaid
flowchart TD
    Start([命令輸入]) --> ParseArgs[解析命令參數<br/>argc/argv]
    ParseArgs --> LookupTable[查詢命令表]
    LookupTable --> Found{命令是否存在?}
    
    Found -->|否| Error[顯示錯誤訊息]
    Found -->|是| RouteCommand{命令路由}
    
    RouteCommand -->|ls| CmdLS[列出目錄內容]
    RouteCommand -->|cd| CmdCD[切換目錄]
    RouteCommand -->|mkdir| CmdMkdir[創建目錄]
    RouteCommand -->|touch| CmdTouch[創建檔案]
    RouteCommand -->|cat| CmdCat[顯示檔案內容]
    RouteCommand -->|echo| CmdEcho[輸出文字<br/>支援重定向]
    RouteCommand -->|rm| CmdRm[刪除檔案/目錄]
    RouteCommand -->|mv| CmdMv[移動/重新命名]
    RouteCommand -->|cp| CmdCp[複製檔案]
    RouteCommand -->|vim| CmdVim[啟動編輯器]
    RouteCommand -->|clear| CmdClear[清屏]
    RouteCommand -->|help| CmdHelp[顯示幫助]
    RouteCommand -->|history| CmdHistory[顯示歷史]
    RouteCommand -->|exit| CmdExit[設定退出標記]
    
    CmdLS --> VFSOp[執行 VFS 操作]
    CmdCD --> VFSOp
    CmdMkdir --> VFSOp
    CmdTouch --> VFSOp
    CmdCat --> VFSOp
    CmdEcho --> VFSOp
    CmdRm --> VFSOp
    CmdMv --> VFSOp
    CmdCp --> VFSOp
    CmdVim --> LaunchEditor[啟動編輯器模式]
    CmdClear --> ShowResult[顯示結果]
    CmdHelp --> ShowResult
    CmdHistory --> ShowResult
    CmdExit --> End([結束])
    
    VFSOp --> ValidatePath[驗證路徑安全性]
    ValidatePath -->|無效| Error
    ValidatePath -->|有效| ExecuteVFSOp[執行 VFS 操作]
    ExecuteVFSOp --> ShowResult
    LaunchEditor --> End
    ShowResult --> End
    Error --> End
    
    style Start fill:#90EE90
    style End fill:#FFB6C1
    style VFSOp fill:#DDA0DD
    style ValidatePath fill:#FFE4B5
    style ExecuteVFSOp fill:#87CEEB
```

## 編輯器主迴圈流程

```mermaid
flowchart TD
    Start([編輯器啟動]) --> InitTerminal[初始化終端<br/>原始模式]
    InitTerminal --> InitScreen[初始化螢幕系統]
    InitScreen --> LoadFile{檔案是否存在?}
    
    LoadFile -->|是| ReadFile[從檔案讀取內容]
    LoadFile -->|否| CreateBuffer[創建空白緩衝區]
    
    ReadFile --> LoadToBuffer[載入到緩衝區]
    CreateBuffer --> LoadToBuffer
    LoadToBuffer --> SetNormalMode[設定為 Normal 模式]
    
    SetNormalMode --> MainLoop{主迴圈}
    MainLoop --> ReadKey[讀取按鍵輸入]
    ReadKey --> GetMode{取得目前模式}
    
    GetMode -->|Normal| HandleNormal[處理 Normal 模式按鍵]
    GetMode -->|Insert| HandleInsert[處理 Insert 模式按鍵]
    GetMode -->|Visual| HandleVisual[處理 Visual 模式按鍵]
    GetMode -->|Command| HandleCommand[處理 Command 模式按鍵]
    
    HandleNormal --> CheckMode{模式是否改變?}
    HandleInsert --> CheckMode
    HandleVisual --> CheckMode
    HandleCommand --> CheckCommand{是否為退出命令?}
    
    CheckCommand -->|:q/:wq| CheckModified{是否有未保存修改?}
    CheckCommand -->|:q!| ExitEditor[退出編輯器]
    CheckCommand -->|其他| ExecuteCommand[執行命令<br/>:w, :e, :b 等]
    
    CheckModified -->|是| PromptSave{提示保存?}
    CheckModified -->|否| ExitEditor
    PromptSave -->|是| SaveFile[保存檔案]
    PromptSave -->|否| ExitEditor
    SaveFile --> ExitEditor
    ExecuteCommand --> UpdateScreen[更新螢幕]
    
    CheckMode -->|是| UpdateScreen
    CheckMode -->|否| UpdateScreen
    
    UpdateScreen --> CheckRunning{編輯器是否運行中?}
    CheckRunning -->|是| MainLoop
    CheckRunning -->|否| CleanupTerminal[清理終端設定]
    
    CleanupTerminal --> End([結束])
    
    style Start fill:#90EE90
    style End fill:#FFB6C1
    style MainLoop fill:#DDA0DD
    style HandleNormal fill:#87CEEB
    style HandleInsert fill:#87CEEB
    style HandleVisual fill:#87CEEB
    style HandleCommand fill:#87CEEB
    style SaveFile fill:#FFE4B5
```

## 編輯器模式切換流程

```mermaid
stateDiagram-v2
    [*] --> Normal: 編輯器啟動
    
    Normal --> Insert: 按 i/a 鍵
    Normal --> Visual: 按 v 鍵
    Normal --> Command: 按 : 鍵
    Normal --> [*]: 執行 :q/:wq/:q!
    
    Insert --> Normal: 按 Esc 鍵
    Insert --> Insert: 輸入文字
    
    Visual --> Normal: 按 Esc 鍵
    Visual --> Visual: 移動游標選取
    
    Command --> Normal: 執行命令後
    Command --> Command: 輸入命令字元
    Command --> [*]: 執行 :q/:wq/:q!
    
    note right of Normal
        預設模式
        支援移動、刪除、複製等操作
    end note
    
    note right of Insert
        文字輸入模式
        直接輸入文字內容
    end note
    
    note right of Visual
        選取模式
        支援字元、行、區塊選取
    end note
    
    note right of Command
        命令模式
        執行 :w, :q, :e 等命令
    end note
```

## VFS 操作流程

```mermaid
flowchart TD
    Start([VFS 操作請求]) --> ValidateInput[驗證輸入參數]
    ValidateInput --> CheckPath{路徑是否有效?}
    
    CheckPath -->|無效| Error[返回錯誤]
    CheckPath -->|有效| NormalizePath[規範化路徑<br/>移除重複斜線]
    
    NormalizePath --> CheckTraversal{檢查路徑遍歷攻擊}
    CheckTraversal -->|偵測到攻擊| Error
    CheckTraversal -->|安全| ParsePath[解析路徑<br/>分割為目錄組件]
    
    ParsePath --> RouteOperation{操作類型}
    
    RouteOperation -->|建立檔案| CreateFile[建立檔案節點]
    RouteOperation -->|建立目錄| CreateDir[建立目錄節點]
    RouteOperation -->|刪除| DeleteNode[刪除節點<br/>遞迴刪除子節點]
    RouteOperation -->|讀取| ReadFile[讀取檔案內容]
    RouteOperation -->|寫入| WriteFile[寫入檔案內容]
    RouteOperation -->|移動| MoveNode[移動節點到新位置]
    RouteOperation -->|重新命名| RenameNode[更新節點名稱]
    RouteOperation -->|列表| ListDir[列出目錄內容]
    
    CreateFile --> FindParent[尋找父目錄]
    CreateDir --> FindParent
    DeleteNode --> FindNode[尋找目標節點]
    ReadFile --> FindNode
    WriteFile --> FindNode
    MoveNode --> FindNode
    RenameNode --> FindNode
    ListDir --> FindNode
    
    FindParent --> CheckExists{父目錄是否存在?}
    FindParent -->|不存在| CreateParents[自動建立父目錄]
    CreateParents --> CreateNode[建立節點]
    CheckExists -->|存在| CreateNode
    CheckExists -->|不存在| CreateParents
    
    FindNode --> NodeFound{節點是否存在?}
    NodeFound -->|否| Error
    NodeFound -->|是| CheckType{檢查節點類型}
    
    CheckType -->|檔案| FileOp[執行檔案操作]
    CheckType -->|目錄| DirOp[執行目錄操作]
    CheckType -->|類型不符| Error
    
    CreateNode --> UpdateStats[更新 VFS 統計資訊]
    FileOp --> UpdateStats
    DirOp --> UpdateStats
    DeleteNode --> UpdateStats
    
    UpdateStats --> Success[返回成功]
    Error --> End([結束])
    Success --> End
    
    style Start fill:#90EE90
    style End fill:#FFB6C1
    style ValidateInput fill:#FFE4B5
    style CheckTraversal fill:#FFE4B5
    style CreateNode fill:#87CEEB
    style FileOp fill:#DDA0DD
    style DirOp fill:#DDA0DD
```

## VFS 持久化流程

```mermaid
flowchart TD
    Start([持久化請求]) --> CheckType{操作類型}
    
    CheckType -->|保存| Serialize[序列化 VFS<br/>轉換為位元組流]
    CheckType -->|載入| ReadFile[讀取加密檔案]
    
    Serialize --> DeriveKey[衍生密鑰<br/>從字串密鑰]
    DeriveKey --> GenerateNonce[產生 Nonce<br/>一次性數值]
    GenerateNonce --> InitChaCha[初始化 ChaCha20<br/>設定密鑰和 Nonce]
    InitChaCha --> Encrypt[加密序列化資料]
    Encrypt --> WriteHeader[寫入檔案標頭<br/>魔數 + 版本號]
    WriteHeader --> WriteData[寫入加密資料]
    WriteData --> Success1[保存成功]
    
    ReadFile --> CheckMagic{檢查魔數}
    CheckMagic -->|無效| Error1[檔案格式錯誤]
    CheckMagic -->|有效| ReadVersion[讀取版本號]
    ReadVersion --> DeriveKey2[衍生密鑰]
    DeriveKey2 --> ReadNonce[讀取 Nonce]
    ReadNonce --> InitChaCha2[初始化 ChaCha20]
    InitChaCha2 --> Decrypt[解密資料]
    Decrypt --> Deserialize[反序列化<br/>還原 VFS 結構]
    Deserialize --> ValidateVFS{驗證 VFS 結構}
    ValidateVFS -->|無效| Error2[資料損壞]
    ValidateVFS -->|有效| Success2[載入成功]
    
    Success1 --> End([結束])
    Success2 --> End
    Error1 --> End
    Error2 --> End
    
    style Start fill:#90EE90
    style End fill:#FFB6C1
    style Serialize fill:#87CEEB
    style Encrypt fill:#DDA0DD
    style Decrypt fill:#DDA0DD
    style DeriveKey fill:#FFE4B5
    style DeriveKey2 fill:#FFE4B5
```

## 緩衝區操作流程

```mermaid
flowchart TD
    Start([緩衝區操作]) --> GetBuffer[取得目前緩衝區]
    GetBuffer --> CheckOp{操作類型}
    
    CheckOp -->|插入字元| InsertChar[在游標位置插入字元]
    CheckOp -->|刪除字元| DeleteChar[刪除游標位置字元]
    CheckOp -->|插入行| InsertLine[在指定行號插入新行]
    CheckOp -->|刪除行| DeleteLine[刪除指定行]
    CheckOp -->|讀取行| GetLine[取得指定行的指標]
    
    InsertChar --> CheckBounds{檢查邊界}
    DeleteChar --> CheckBounds
    InsertLine --> CheckBounds
    DeleteLine --> CheckBounds
    GetLine --> CheckBounds
    
    CheckBounds -->|超出範圍| Error[返回錯誤]
    CheckBounds -->|有效| AllocMem[分配記憶體]
    
    AllocMem --> UpdateLine[更新行內容]
    UpdateLine --> UpdateLinks[更新鏈結串列指標]
    UpdateLinks --> UpdateCount[更新行數計數]
    UpdateCount --> MarkModified[標記為已修改]
    MarkModified --> Success[返回成功]
    
    Error --> End([結束])
    Success --> End
    
    style Start fill:#90EE90
    style End fill:#FFB6C1
    style CheckBounds fill:#FFE4B5
    style AllocMem fill:#87CEEB
    style UpdateLinks fill:#DDA0DD
    style MarkModified fill:#FFE4B5
```

## 安全驗證流程

```mermaid
flowchart TD
    Start([使用者輸入]) --> ValidateString[驗證字串長度]
    ValidateString --> LengthOK{長度是否有效?}
    
    LengthOK -->|否| Error1[長度超出限制]
    LengthOK -->|是| ValidateChars[驗證字元有效性]
    
    ValidateChars --> CharsOK{字元是否有效?}
    CharsOK -->|否| Error2[包含無效字元]
    CharsOK -->|是| CheckPath{是否為路徑?}
    
    CheckPath -->|是| SanitizePath[清理路徑]
    CheckPath -->|否| CheckBuffer{是否為緩衝區操作?}
    
    SanitizePath --> RemoveDanger[移除危險字元]
    RemoveDanger --> NormalizePath[規範化路徑]
    NormalizePath --> CheckTraversal{檢查路徑遍歷}
    
    CheckTraversal -->|偵測到攻擊| Error3[路徑遍歷攻擊]
    CheckTraversal -->|安全| ValidatePathLen[驗證路徑長度]
    
    ValidatePathLen --> PathOK{路徑長度有效?}
    PathOK -->|否| Error4[路徑過長]
    PathOK -->|是| CheckBuffer
    
    CheckBuffer -->|是| ValidateBounds[驗證緩衝區邊界]
    CheckBuffer -->|否| Success[驗證通過]
    
    ValidateBounds --> BoundsOK{邊界是否有效?}
    BoundsOK -->|否| Error5[緩衝區溢位]
    BoundsOK -->|是| Success
    
    Error1 --> End([結束])
    Error2 --> End
    Error3 --> End
    Error4 --> End
    Error5 --> End
    Success --> End
    
    style Start fill:#90EE90
    style End fill:#FFB6C1
    style ValidateString fill:#FFE4B5
    style CheckTraversal fill:#FF6B6B
    style ValidateBounds fill:#FFE4B5
    style Success fill:#90EE90
```

## 模組依賴關係

```mermaid
graph TD
    Main[main.c] --> Shell[shell.c]
    Main --> Editor[editor.c]
    
    Shell --> ShellCommands[shell_commands.c]
    Shell --> VFS[vfs.c]
    Shell --> VFSPersist[vfs_persist.c]
    Shell --> Error[error.c]
    Shell --> Memory[memory.c]
    
    Editor --> Buffer[buffer.c]
    Editor --> Command[command.c]
    Editor --> VimOps[vim_ops.c]
    Editor --> Screen[screen.c]
    Editor --> Input[input.c]
    Editor --> Error
    
    ShellCommands --> VFS
    ShellCommands --> Path[path.c]
    ShellCommands --> FileOps[fileops.c]
    
    VFS --> Validation[validation.c]
    VFS --> Sanitize[sanitize.c]
    VFS --> Memory
    
    VFSPersist --> VFS
    VFSPersist --> ChaCha20[chacha20.c]
    VFSPersist --> Memory
    
    Buffer --> Memory
    Buffer --> Error
    
    VimOps --> Buffer
    VimOps --> Screen
    VimOps --> Input
    
    Screen --> Buffer
    Screen --> Colors[colors.h]
    
    Input --> Error
    
    Path --> Validation
    Path --> Sanitize
    
    FileOps --> Path
    FileOps --> Validation
    FileOps --> Sanitize
    
    style Main fill:#FFB6C1
    style Shell fill:#87CEEB
    style Editor fill:#87CEEB
    style VFS fill:#DDA0DD
    style Security fill:#FFE4B5
    style UI fill:#90EE90
```

