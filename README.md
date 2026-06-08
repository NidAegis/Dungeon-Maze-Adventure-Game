\# 地城迷宮冒險遊戲 Dungeon Maze Adventure Game

## 如何執行

本程式建議在 Windows 環境執行，並需要先安裝 GCC / MinGW。

---

## 1. 開啟專案資料夾

用 VS Code 或終端機進入本專案資料夾。

例如：

```powershell
cd Downloads\Dungeon-Maze-Adventure-Game-main
```

---

## 2. 編譯程式

請輸入以下指令：

```powershell
gcc -std=c99 -finput-charset=UTF-8 -fexec-charset=UTF-8 "dungeon_maze_adventure.c" -o dungeon_maze_adventure.exe
```

---

## 3. 執行程式

如果要直接在目前終端機執行：

```powershell
chcp 65001
.\dungeon_maze_adventure.exe
```

如果要像 Dev-C++ 一樣開新視窗執行：

```powershell
cmd /c start "Dungeon Maze Adventure" cmd /k "chcp 65001 >nul && dungeon_maze_adventure.exe"
```

---

## 注意事項

- 請使用英文輸入法操作遊戲。
- 如果中文顯示亂碼，請確認有先輸入 `chcp 65001`。
- 如果編譯失敗，請確認電腦已安裝 GCC / MinGW。


本專題使用 C 語言製作文字介面的地城迷宮冒險遊戲，玩家需要在迷宮中移動、探索、避開危險並完成遊戲目標。



\## 使用技術

\- C 語言

\- 檔案讀取

\- 陣列

\- 函式模組化

\- 遊戲流程控制

\- 文字介面互動



\## 功能特色

\- 迷宮地圖讀取

\- 玩家移動控制

\- 地圖顯示

\- 遊戲勝敗判定

\- 多組迷宮測試資料



\## 專題展示

YouTube Demo：



\[https://www.youtube.com/watch?v=V7Y--qQHBd4]



\## 專案文件

\- Report.docx

\- Demo.pptx

