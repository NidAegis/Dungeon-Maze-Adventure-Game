\# 地城迷宮冒險遊戲 Dungeon Maze Adventure Game

## 如何執行
本程式建議在 Windows 環境執行，並需要先安裝 GCC。

---

## 1. 安裝 GCC

開啟 CMD，輸入：

```cmd
winget install MSYS2.MSYS2
```

安裝完成後，開啟「MSYS2 UCRT64」，輸入：

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc
```

接著將以下路徑加入 Windows 環境變數 Path：

```text
C:\msys64\ucrt64\bin
```

重新開啟 CMD 後，輸入以下指令確認是否安裝成功：

```cmd
gcc --version
```

如果有顯示版本資訊，代表安裝成功。

---

## 2. 進入專案資料夾

```cmd
cd C:\Users\你的使用者名稱\Downloads\Dungeon-Maze-Adventure-Game-main
```

---

## 3. 編譯程式

```cmd
gcc -std=c99 -finput-charset=UTF-8 -fexec-charset=UTF-8 "dungeon_maze_adventure.c" -o dungeon_maze_adventure.exe
```

---

## 4. 開新視窗執行遊戲

```cmd
cmd /c start "Dungeon Maze Adventure" cmd /k "chcp 65001 >nul && dungeon_maze_adventure.exe"
```

## 注意事項

- 請使用英文輸入法操作遊戲。
- 如果出現 `gcc 不是內部或外部命令`，代表 GCC 尚未安裝或 Path 尚未設定。


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

