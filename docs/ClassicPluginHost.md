# ClassicPluginHost：原版 UTAU 插件的支持

本文档是 ClassicPluginHost 插件的计划，写于 2026-09-30，已实现，待作者验收（实施步骤 5）。作者已于同日决定全部待定事项，见末尾。插件机制见 [`Plugins.md`](Plugins.md)，插件的定义以 [`note.md`](note.md) 为准。

## 目标

原版 UTAU 插件是一个文件夹，含 `plugin.txt` 与一个可执行文件。UTAU 将选区写入一个临时文件，以其路径为第一个参数启动插件，等待插件结束，读回插件改写后的文件并应用其中的修改。验收同 Roadmap 第五阶段：若干社区常用的原版插件能够正常执行并写回结果。

按 note.md，这一支持本身是一个原生插件（ClassicPluginHost）：它发现 UTAU 插件，将每个插件作为一项命令加入「工具 → Classic Plugins」菜单，运行时修改选区；应用本身不含 UTAU 插件的代码。

## 调研

### 规格

官方规格为 <https://w.atwiki.jp/utaou/pages/64.html>（「プラグイン仕様」，页面最后更新 2019-01-04）。要点：

- **`plugin.txt`**：`name`（菜单文字）、`execute`（可执行文件名）、`shell=use`（以 `ShellExecuteEx` 代替 `CreateProcess`，可运行 jar、html、hta 等）、`ustversion`（0.4.15 起，临时文件的条目格式 1.00 / 1.10 / 1.20，省略时用 UTAU 的设置）、`notes`（0.4.15 起，「指定した場合」忽略选区、传入全部音符）。**规格中没有编码的键。**
- **临时文件**：规格写作 Shift_JIS，实为本地编码（规格作者使用日文系统；实测见下），CRLF。规格不规定文件名与位置。插件改写同一文件作为结果。
  - `[#VERSION]`（`UST Version 1.20`）、`[#SETTING]`（只读，输出时可省略）。
  - 编号段落是所选音符。**编号没有意义，输出的段落按出现顺序应用到选区**。
  - `[#PREV]` / `[#NEXT]` 是选区前后各一个音符，没有时省略；输出中若出现，其修改被应用。
  - 输出专用：`[#INSERT]` 在该位置插入音符（算作一个编号段落，写在 PREV 之前或 NEXT 之后也不会插入到选区外）；`[#DELETE]` 写在某编号段落的位置即删除该音符，其他段落不能删除。
  - 省略的条目表示不变，未改的音符可以只写段落头。`[#INSERT]` 省略的 `Length`、`Lyric`、`NoteNum` 取自后一个音符。
  - 所有段落都省略表示取消。
  - 只读条目 `@preuttr`、`@overlap`、`@stpoint`、`@filename`、`@alias`、`@cache` 在输出中被忽略；条目名有 `Moduration`/`Modulation`、`Piches`/`Pitches`/`PitchBend` 等变体。

### 实测

2026-09-30 以 utau.exe 0.04.0018 实测，记录见 [`claude/utau-plugin-protocol.md`](claude/utau-plugin-protocol.md)：

- 临时文件为 `%TEMP%\utau1\tmpXXXX.tmp`，系统 ANSI 代码页，CRLF，`UST Version 1.20`，`[#SETTING]` 为绝对路径。
- **编号段落以轨道位置编号**（部分选区不从 0 起）；`[#PREV]`/`[#NEXT]` 是完整的音符，选区在开头或结尾时省略；`notes=all` 传入全部音符，不写 `[#PREV]`/`[#NEXT]`。
- 结果**按出现顺序**应用；写在 `[#PREV]` 之前或 `[#NEXT]` 之后的 `[#INSERT]` 插在选区的起点或终点，不越出选区；`[#PREV]`/`[#NEXT]` 的修改被应用；省略的条目不变，空值恢复默认；`[#INSERT]` 的长度与音高取自后一个音符；空文件为取消。与下文的设计一致。

### 参考实现

| 方面 | OpenUtau（`.cache/OpenUtau`，a60ca58） | QSynthesis（`.cache/QSynthesis-Old`，7b96f2d） |
|---|---|---|
| 发现 | `<数据目录>/Plugins` 下递归查找 `plugin.txt` | `<程序目录>/plugins` 的直接子文件夹，目录监视自动重载 |
| `plugin.txt` 编码 | 固定 Shift_JIS；自定 `encoding=` 键决定临时文件编码 | BOM、UTF-8，否则 GBK；自定 `charset=` 键 |
| 临时文件 | 固定 `<缓存>/temp.tmp`；无 `[#VERSION]`；不写 Mode1 与包络 | 每次唯一的临时目录与文件名，读回后删除；写全部条目与 PREV/NEXT |
| 启动 | `UseShellExecute` 按 `shell`，工作目录为插件文件夹；无超时、不能取消 | `CreateProcess`（`CREATE_NEW_CONSOLE`，控制台可见）或 `ShellExecuteEx`；模态对话框有「取消」但不结束进程 |
| 读回 | 按顺序应用；忽略 PREV/NEXT 的修改；文件未变即不做任何事 | 按编号应用（与规格不符）；应用 PREV/NEXT 的修改 |
| 撤销 | 一个撤销组 | 一个历史步骤 |

两者都有缺陷（`notes=all` 实际都未生效；OpenUtau 的 Wine 判断与 NEXT 的间隙计算有误；QSynthesis 按编号映射），**只参考思路**。

### stdutau

`PluginTxt` 可直接用于读取 `plugin.txt`（原始字节，编码与路径校验由宿主负责）。原有的 `PluginFileReader` / `PluginFileWriter` 是**插件一侧**的读写，不能用于宿主：`PluginFileReader` 忽略 `[#INSERT]`/`[#DELETE]`，丢弃只有段落头的音符，且在省略 `PBS` 时清空 Mode2 音高，违反「省略即不变」。另有两处与规格或实测不符，已修正（见实施步骤 2）：`PluginFileWriter` 的 `prependNotesBeforePrev` / `appendNotesAfterNext` 的说明称其插入到选区外；`PluginFileReader` 关于「临时文件总从 0 编号」的注释与实测相反（以轨道位置编号）。

## 设计

### 分层

除协议外全部实现在 ClassicPluginHost 插件中，HelloUtau 的库中不为它新开模块（作者 2026-09-30 定）。「选区编辑」的通用扩展点待出现第二个使用者时再从插件中提炼。

1. **协议（stdutau `pluginfile.h`，`2004f8e`）**：一个文件一个类，插件与宿主共用（作者定），都在原始字节上工作。
   - `PluginInput`，传给插件的临时文件：插件读，宿主写。写出 `[#VERSION]`（`UST Version 1.20`）、`[#SETTING]`（Project、选区起点的 Tempo、绝对 VoiceDir 与 CacheDir、Mode2）、`[#PREV]`、以轨道位置编号的音符（带只读的 `@` 条目）、`[#NEXT]`，CRLF。
   - `PluginResult`，插件写回的结果：插件写，宿主读。数据是按出现顺序的段落列表（编号、`[#INSERT]`、`[#DELETE]`、`[#PREV]`/`[#NEXT]`），每段记录**出现过的条目**（`keys`）与其值（`note`），空值的条目保持空白，供宿主在原音符上逐条合并；没有音符段落即取消（`isCancelled()`）。`PluginResult(input)` 为全部不变的结果，`Section::assign()` 设置一个音符。
2. **ClassicPluginHost 插件**：
   - 名称（作者 2026-09-30 定）：目录与工程 `ClassicPluginHost`，插件类 `ClassicPluginHostPlugin`（`Internal`，不导出），ID `org.helloutau.classicpluginhost`，显示名 Classic Plugin Host。
   - 库一级的三个类（公开、有测试）：`ClassicPlugin`（读 `plugin.txt`、发现、`execute` 路径校验、平台是否可用）；`ClassicPluginExchange`（`input()` 写出临时文件，`apply()` 在一个 `EditSession::Transaction` 中按段落顺序合并结果，整个结果为**一个撤销步骤**）；`ClassicPluginRunner`（临时文件、启动、等待或取消、读回）。OpenUtau 同样分为 `PluginLoader` 与 `PluginRunner`。
   - 内部：`ClassicPluginContribution` 以一个 `ActionContribution` 向工程窗口加入「工具 → Classic Plugins」子菜单（同 UTAU，在「清除渲染缓存」之后）：清单中是一个 external 动作，登记其 `QMenu` 的 `menuAction()`，每次打开时填入发现的 UTAU 插件（不可用的显示为灰色，提示原因），其后是「刷新」「打开插件目录」与「打开 UTAU 插件目录」。菜单名为 Classic Plugins，以免与原生插件混淆（作者 2026-09-30 定）。同 UTAU 在卷帘中按 N 于鼠标处弹出插件菜单，命令 Classic Plugins at Pointer（快捷键 N）在鼠标处弹出同一菜单。该命令不在任何菜单中，只能以快捷键或命令面板执行（作者 2026-09-30 定）。焦点在文字输入框中时 N 仍为输入。`runClassicPlugin()` 取选区、确认、写出、运行（模态对话框，「取消」，结束无法观察时另有「完成」）、合并并报告。
   - 运行不另开线程：进程本身即为异步，等待由事件循环中的通知完成（Windows 为进程句柄的 `QWinEventNotifier`，其他平台为 `QProcess`）。
3. **HelloUtauEditor**：仅补充插件所需的能力，不新开模块。`ProjectWindow` 公开其卷帘（`pianoRoll()`）与 `editor()`（用于读取设置中的 UTAU 文件夹），符合 Plugins.md「编辑器的组件化」中「窗口公开能力」的方向。UTAU 的选区是连续的一段，卷帘的选区不一定连续，插件取第一个到最后一个所选音符的范围。编辑器移除贡献的动作时，external 条目的动作是其菜单的 `menuAction()`，归菜单所有，因此删除菜单而不是单独删除动作。

### 已按规格或约定确定的做法（作者可推翻）

- 结果**按段落出现的顺序**应用，不看编号（规格原文）。
- 结果读回时，任意一串连续的 CR、LF 都算作一个行尾，由 stdutau 的 `takeLine()` 处理。UTAU 读回插件结果时对 CRLF、`\r\r\n`（以 Windows 文本流写出 `"\r\n"` 的旧插件常见）、只有 LF、只有 CR 四种行尾的处理完全相同（2026-10-06 实测，`.cache/utau-probe/unverified` 探针 8）。
- `[#PREV]`/`[#NEXT]` 的修改**应用**（规格原文），与选区的修改同在一个撤销步骤中。
- 合并按条目：省略的条目不变；空值（如 `Intensity=`）按规格恢复默认。
- 文件内容未变，或所有段落都省略，视为取消，不产生撤销步骤。
- `ustversion` 省略时按 1.20 写出，与实测一致。`notes` 只要出现即传入全部音符（规格「指定した場合」，同 stdutau）。
- 临时文件每次运行唯一，位于系统临时目录下的一个子目录，读回后删除；名称同 UTAU 为 `tmpXXXX.tmp`。
- `execute` 是不可信路径：拒绝绝对路径、`..` 与解析后落在插件文件夹以外的路径；临时文件路径只作为参数数组的一项传入，不拼接命令行（CLAUDE.md）。工作目录为插件文件夹。
- 启动：Windows 上以 `CreateProcess` 挂起启动、放入作业对象后再恢复，插件启动的进程都在作业中，「取消」以 `TerminateJobObject` 结束整棵进程树；`shell=use` 以 `ShellExecuteEx`，其进程同样放入作业。控制台窗口可见，同 UTAU。批处理（`.bat`、`.cmd`）以 `cmd.exe /d /s /c ""程序" "文件""` 启动：直接交给 `CreateProcess` 时 Windows 以 `cmd /c` 执行整行，会去除首尾引号而破坏路径；路径含 `%` 时拒绝运行，因为引号内 cmd 仍会展开变量。其他平台以 `QProcess` 在独立的进程组中启动，「取消」结束整个进程组。
- 插件的用户目录为应用数据目录下的 `UtauPlugins`（Windows 上为 `%APPDATA%\OpenVPI\HelloUtau\UtauPlugins`，Qt 依次附加组织名与应用名），与原生插件的目录分开。**插件文件夹依次为该用户目录与设置中 UTAU 文件夹的 `plugins` 目录**（作者 2026-10-08 决定，与 note.md「音源文件夹」的顺序相同），不存在的文件夹视同未列出；两处的插件都列在菜单中，按文件夹的顺序排列。插件在菜单第一次打开、「刷新」与设置中的 UTAU 文件夹改变后重新发现。
- 首次运行的确认记在插件设置 `plugins.json` 中本插件的值（`AppLoader::pluginValue()`）：`userData/org.helloutau.classicpluginhost/approved` 为数组，每项是插件文件夹 `folder`、程序在插件文件夹中的相对路径 `relativePath`（仅供阅读）与程序内容的 SHA-256 `sha256`；仅比较 `sha256`，程序改变后再次询问。
- 选区为空时提示先选择音符（`notes` 插件除外）；读回的文件与写出的相同时视为取消。
- 编码按 note.md「插件」：`plugin.json` 的 `charset` 声明 UTF-8 时，临时文件写成 UTF-8、结果按 UTF-8 读回（作者 2026-09-30 定，比 UTAU 稳妥）；否则与 UTAU 兼容，Windows 上为系统 ANSI 代码页、其他平台为 CP932，音符条目中无法表示的字符按 note.md 转义；`[#SETTING]` 的路径与 `@` 条目只读，不转义，路径用系统的分隔符，同 UTAU。写出与读回用同一编码。`plugin.json` 的格式见 note.md「插件」，有它时不再读 `plugin.txt`。

## 实施步骤

1. ~~**探针**~~：已完成（2026-09-30），见 [`claude/utau-plugin-protocol.md`](claude/utau-plugin-protocol.md)。
2. **stdutau**：
   - ~~插件一侧的修正~~（stdutau `ae46c5c`）：删除 `prependNotesBeforePrev` / `appendNotesAfterNext`（作者定；实测插入不越出选区，二者等于在选区两端 `insertNotes()`）；`PluginFileReader::load()` 以第一个编号段落的编号为 `startIndex`。
   - ~~宿主一侧~~（stdutau `2004f8e`）：原来插件一侧的两个类与宿主所需的两半合并为 `PluginInput` 与 `PluginResult`（作者定，不保留旧名），测试以实测样本与往返为依据。
3. ~~**HelloUtauEditor**~~：已完成（`f6faf4b`）。`ProjectWindow::pianoRoll()` 公开卷帘，选区由它的 `selectedIndices()` 取得、`selectionChanged()` 跟踪；卷帘随文档替换，替换后发出 `ProjectWindow::documentChanged()`。
4. ~~**ClassicPluginHost**~~：发现、`plugin.txt` 与编码、写出与合并（`fab6818`）；进程的启动与取消（`ad8da63`）、菜单、模态对话框、首次确认（`f045b9e`）。库一级的部分在 `tests/auto/plugins/ClassicPluginHost` 中测试：合并以探针 R1–R3 的结果对照 UTAU 另存的工程；运行以批处理、`shell=use` 与测试程序自身充当的 `.exe` 插件端到端运行，包括取消时结束插件启动的后台进程。菜单与对话框由作者试用。
   - `plugin.json`（作者定的格式，见 note.md「插件」）：已实现。
   - 未做：`ustversion` 1.00 与 1.10 的条目名（现在一律按 1.20 写出，其格式未经实测）。
5. **验收**：作者以社区常用的原版插件试用。

## 作者的决定（2026-09-30）

- stdutau 中插件一侧原有的读写不用于宿主（后改为插件与宿主共用 `PluginInput` / `PluginResult`，见实施步骤 2）。
- **`shell=use`**：Windows 上按照 UTAU 的做法使用 `ShellExecuteEx`。处理程序不返回进程句柄时（html、hta 可能交给已打开的程序）无法等待，提示用户在插件完成后手动确认。CLAUDE.md 中的「尚未确定」随之解决。
- **非 Windows 平台上的 `.exe` 插件**：第一版在菜单中显示但不可用，并说明原因。Wine 留待以后决定。
- **发现**：设置中 UTAU 文件夹下的 `plugins`（一层，同 UTAU），另加 HelloUtau 自己的用户目录（与原生插件的目录分开）。启动时与「刷新」时重新扫描。
- **取消**：「取消」结束插件的整个进程树并丢弃结果，不设超时。
- **退出码**：不检查，仅以文件为结果。
- **协议代码放在 stdutau**，与 `PluginTxt`、插件一侧原有的读写同处。stdutau 原有的是协议的插件一半（读输入、写结果），宿主所需的是另一半：
  - 输入的写出：由设置、前后音符与选区写出 `[#VERSION]`、`[#SETTING]`、`[#PREV]`、带 `@` 条目的编号音符、`[#NEXT]`，CRLF；
  - 结果的读取：将插件写回的文件解析为按顺序的段落（编号音符、`[#INSERT]`、`[#DELETE]`、`[#PREV]`/`[#NEXT]`），并记录每段出现过的条目，供宿主逐条合并。
  - 插件一侧写出的结果正是宿主读取的输入，可作测试的对照。
- **首次运行的确认**：新发现的插件第一次运行前弹出一次确认（插件名与将运行的程序），确认过的记入设置，不再询问；插件的程序改变后重新询问。
