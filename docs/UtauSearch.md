# 搜索 UTAU 文件夹

本文档定义设置的「UTAU」页中「搜索...」按钮的行为：在后台查找本机的 UTAU 文件夹，列出候选，由用户选择一个填入「UTAU 文件夹」（作者 2026-10-21 提出）。

## 动机

UTAU 文件夹决定合成工具的相对路径、`%VOICE%` 与 UTAU 的插件文件夹，见 [`note.md`](note.md)。多数用户不知道 UTAU 装在哪里，免安装版更是放在任意位置。搜索只提供候选，不自动填入，因为同一台机器上常有多个副本（官方版、汉化版、不同版本），选哪一个由用户决定。

## 官方安装包的事实

以下由 `utaustup.msi` 0.4.18 与 0.4.19 的数据库与一次实际安装确认（2026-10-21，作者以 0.4.19 安装到 `D:\UTAU-test`，选「只为我」）：

- 默认安装目录由自定义动作 `DIRCA_TARGETDIR` 设为 `[ProgramFilesFolder]UTAU`。安装包是 32 位的，在 64 位系统上即 `C:\Program Files (x86)\UTAU`。用户可在安装时修改。
- 安装包不写任何注册表项（`Registry` 表为空），不关联 `.ust`（`Extension` 表为空）。
- 卸载信息写在 `HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\<ProductCode>` 下，选「只为我」时同样如此。其中的 `InstallLocation` 与 `DisplayIcon` 为空，`InstallSource` 是安装包所在的目录而非安装目录，因此卸载信息找不到安装位置。
- 两个版本的 `UpgradeCode` 同为 `{BB390196-0E40-444A-82E2-E7A8A02A99F5}`，`utau.exe` 所在组件的 `ComponentId` 同为 `{14A967E4-7732-B0D9-1011-92F5FDA8A4EC}`。`MsiEnumRelatedProducts()` 按 `UpgradeCode` 列出安装的产品，`MsiGetComponentPath()` 返回该组件在每个产品中的 `utau.exe`，实测为 `D:\UTAU-test\utau.exe`。
- `utau.exe` 的版本资源中，`FileVersion` 为 `0.04.0018` 或 `0.04.0019`。汉化版的 `ProductName` 为「歌声合成软件 - UTAU」，原版为「歌声合成ツール UTAU」。

## 来源

按以下顺序查找，同一文件夹只列一次，取最先的来源。每个候选必须含有 `utau.exe`，否则不列出。

1. **安装记录**（Windows）：Windows Installer 中 `UpgradeCode` 为上述值的产品，以 `MsiGetComponentPath()` 取得 `utau.exe`。只为我与为所有用户的安装都能找到，与安装时选的目录无关。
2. **常见位置**：Windows 上每个固定磁盘（`GetDriveTypeW()` 为 `DRIVE_FIXED`）的 `Program Files (x86)\UTAU`、`Program Files\UTAU` 与根上的 `UTAU`，以及 `%LOCALAPPDATA%\Programs\UTAU`。只检查这些文件夹，不递归扫描。网络磁盘、光驱与可移动磁盘不检查，因为访问可能长时间无响应。
3. **运行记录**（Windows）：Windows 为运行过的程序记录的路径，即 `HKCU\SOFTWARE\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Compatibility Assistant\Store` 的值名，以及 `HKCU\SOFTWARE\Classes\Local Settings\Software\Microsoft\Windows\Shell\MuiCache` 的值名去掉 `.FriendlyAppName` 等后缀。文件名为 `utau.exe` 的路径取其所在文件夹。这两处没有 Microsoft 的公开文档，作者 2026-10-21 决定采用，因为免安装版只能由此找到：本机实测找到了全部 4 个运行过的 `utau.exe`，包括下载目录中的汉化版。键或值不存在时跳过，不报告错误。
4. **Wine**（macOS 与 Linux，作者 2026-10-21 决定）：`$WINEPREFIX`，或者未设置时的 `~/.wine`，其中 `drive_c` 下的 `Program Files (x86)/UTAU`、`Program Files/UTAU` 与 `UTAU`。不查找其他前缀。

## 时限与线程

搜索在后台线程中进行，候选逐个加入列表。对话框打开 1 秒后不再等待，状态行报告搜索已结束，之后找到的候选不再加入。上述来源都不递归，正常情况下远在 1 秒内完成，时限针对的是休眠中的硬盘等个别慢的访问。线程的结果经排队的信号送达对话框，对话框关闭后线程自行结束并删除。

## 界面

- 「UTAU 文件夹」一行中「浏览...」之前的「搜索...」按钮打开对话框。
- 对话框列出候选，每行为文件夹、版本与来源。版本取自 `utau.exe` 的 `FileVersion`，写作 `0.4.19`，产品名不是原版的名字时附在后面，因此能区分汉化版。macOS 与 Linux 上不读版本资源，版本一栏为空。
- 用户选中一行后点「使用此文件夹」或双击该行，文件夹填入「UTAU 文件夹」，在设置应用时生效。没有候选时，状态行提示用「浏览...」选择。

## 实现结构

- **`UtauFolderSearch`**（`HelloUtauEditor`，作者 2026-10-21 决定放在编辑器中）：各来源的查找与候选的确认、去重。可在测试中替换输入的部分（常见位置所用的磁盘、Wine 前缀、运行记录中的路径）是独立的静态函数。Windows 上链接系统库 `msi` 与 `version`。
- **`UtauFolderSearchDialog`**（`HelloUtauEditor`）：后台线程、1 秒的时限与列表。
- 测试为 `test_UtauFolderSearch` 与 `test_UtauFolderSearchDialog`。安装记录与注册表无法在 CI 中构造，只测试由路径得到候选的部分。