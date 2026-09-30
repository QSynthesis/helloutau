# ClassicPlugin：原版 UTAU 插件的支持

本文档是 ClassicPlugin 插件与「选区编辑」扩展点的计划，写于 2026-09-30，尚未实现。作者已于同日决定全部待定事项，见末尾。插件机制见 [`Plugins.md`](Plugins.md)，插件的定义以 [`note.md`](note.md) 为准。

## 目标

原版 UTAU 插件是一个文件夹，含 `plugin.txt` 与一个可执行文件。UTAU 把选区写入一个临时文件，以其路径为第一个参数启动插件，等待插件结束，读回插件改写后的文件并应用其中的修改。验收同 Roadmap 第五阶段：若干社区常用的原版插件能够正常执行并写回结果。

按 note.md，这一支持本身是一个原生插件（ClassicPlugin）：它发现 UTAU 插件，把每个登记为一项**选区编辑**；应用本身不含 UTAU 插件的代码。

## 调研

### 规格

官方规格为 <https://w.atwiki.jp/utaou/pages/64.html>（「プラグイン仕様」，页面最后更新 2019-01-04）。要点：

- **`plugin.txt`**：`name`（菜单文字）、`execute`（可执行文件名）、`shell=use`（以 `ShellExecuteEx` 代替 `CreateProcess`，可运行 jar、html、hta 等）、`ustversion`（0.4.15 起，临时文件的条目格式 1.00 / 1.10 / 1.20，省略时用 UTAU 的设置）、`notes`（0.4.15 起，「指定した場合」忽略选区、传入全部音符）。**规格中没有编码的键。**
- **临时文件**：Shift_JIS，CRLF。规格不规定文件名与位置。插件改写同一文件作为结果。
  - `[#VERSION]`（`UST Version 1.20`）、`[#SETTING]`（只读，输出时可省略）。
  - 编号段落是所选音符。**编号没有意义，输出的段落按出现顺序应用到选区**。
  - `[#PREV]` / `[#NEXT]` 是选区前后各一个音符，没有时省略；输出中若出现，其修改被应用。
  - 输出专用：`[#INSERT]` 在该位置插入音符（算作一个编号段落，写在 PREV 之前或 NEXT 之后也不会插到选区外）；`[#DELETE]` 写在某编号段落的位置即删除该音符，其他段落不能删除。
  - 省略的条目表示不变，未改的音符可以只写段落头。`[#INSERT]` 省略的 `Length`、`Lyric`、`NoteNum` 取自后一个音符。
  - 所有段落都省略表示取消。
  - 只读条目 `@preuttr`、`@overlap`、`@stpoint`、`@filename`、`@alias`、`@cache` 在输出中被忽略；条目名有 `Moduration`/`Modulation`、`Piches`/`Pitches`/`PitchBend` 等变体。

### 实测

`.cache/utau-probe/voicedir/results/dump-*.ust` 是 utau.exe 0.04.0018 在**全选**时传给探针插件的临时文件（记录见 [`claude/utau-voicedir-cachedir.md`](claude/utau-voicedir-cachedir.md)）：Shift_JIS、CRLF，`[#VERSION]` 为 `UST Version 1.20`，`[#SETTING]` 为 `Project`、`Tempo`、`VoiceDir`、`CacheDir`（均为解析后的绝对路径）、`Mode2`，音符从 `[#0000]` 编号并带只读的 `@` 条目。**尚无部分选区、`[#PREV]`/`[#NEXT]`、`notes=` 的样本，临时文件的路径与名称也未记录。**

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

`PluginTxt` 可直接用于读 `plugin.txt`（原始字节，编码与路径校验由宿主负责）。`PluginFileReader` / `PluginFileWriter` 是**插件一侧**的读写，不能用于宿主：读者忽略 `[#INSERT]`/`[#DELETE]`，丢弃只有段落头的音符，且在省略 `PBS` 时清空 Mode2 音高，违反「省略即不变」。另有两处与规格不符，须一并修正：`PluginFileWriter::prependNotesBeforePrev` / `appendNotesAfterNext` 声称插到选区外；`PluginFileReader` 关于「临时文件总从 0 编号」的注释没有依据。

## 设计

### 分层

1. **协议（stdutau）**：宿主一侧的两部分，都在原始字节上工作。
   - 临时文件的写出：`[#VERSION]`、`[#SETTING]`（Project、选区起点的 Tempo、绝对 VoiceDir 与 CacheDir、Mode2）、`[#PREV]`、编号音符（带只读的 `@` 条目）、`[#NEXT]`，CRLF。
   - 结果的解析：按出现顺序的段落序列，编号段落、`[#INSERT]`、`[#DELETE]`、`[#PREV]`/`[#NEXT]`，每个段落只记录**出现过的条目**，供宿主在原音符上逐条合并；全部省略即取消。
2. **选区编辑扩展点（HelloKitEdit）**：不涉及界面，可测试。
   - `RangeEdit`：`id()`、`name()`、`run(input, cancel, diagnostics)`。输入是工程的只读快照与选区（加上是否要全部音符由编辑自己决定）；输出是「以这些音符替换这一段，另可改选区前后各一个音符」，或取消。`run` 在工作线程上执行。
   - `RangeEditRegistration(std::unique_ptr<RangeEdit>)` 与 `RangeEditRegistrations`，形状同 Plugins.md「注册接口」。
   - 应用输出为**一个撤销步骤**（需要一个领域函数，替换一段音符并可改前后音符）。
3. **ClassicPlugin 插件**：发现 UTAU 插件，为每个登记一个 `RangeEdit`。其 `run`：校验 `execute` 路径、按编码写出临时文件、启动进程并等待（可取消）、读回、解析、合并为输出。
4. **应用（HelloUtauEditor）**：工程窗口的「工具 → 插件」子菜单（同 UTAU），列出已登记的选区编辑，其后是「刷新」与「打开插件目录」。运行期间显示模态对话框（插件名、「取消」），结束后应用结果或报告错误。

### 已按规格或约定确定的做法（作者可推翻）

- 结果**按段落出现的顺序**应用，不看编号（规格原文）。
- `[#PREV]`/`[#NEXT]` 的修改**应用**（规格原文），与选区的修改同在一个撤销步骤中。
- 合并按条目：省略的条目不变；空值（如 `Intensity=`）按规格恢复默认。
- 文件内容未变，或所有段落都省略，视为取消，不产生撤销步骤。
- `ustversion` 省略时按 1.20 写出，与实测一致。`notes` 只要出现即传入全部音符（规格「指定した場合」，同 stdutau）。
- 临时文件每次运行唯一，位于系统临时目录下的一个子目录，读回后删除；扩展名 `.tmp`，同 UTAU 系插件的惯例。
- `execute` 是不可信路径：拒绝绝对路径、`..` 与解析后落在插件文件夹以外的路径；临时文件路径只作为参数数组的一项传入，不拼接命令行（AGENTS.md）。工作目录为插件文件夹。
- 编码按 note.md：插件文件夹有 `plugin.json` 时按其中的编码，否则 Windows 上为系统 ANSI 代码页、其他平台为 CP932；目标编码无法表示的字符按 note.md 转义。写出与读回用同一编码。

## 实施步骤

1. **探针**：用 utau.exe 实测部分选区、选区在开头与结尾、`notes=all`、休止符与间隙时的临时文件，补齐样本（`.cache/utau-probe`），并记录临时文件的路径与名称。
2. **stdutau**：宿主一侧的写出与解析，测试以实测样本与 `PluginFileWriter` 的输出为依据；修正上文两处与规格不符的地方。单独提交到 stdutau。
3. **选区编辑扩展点**：`RangeEdit` 与登记、领域函数、测试。
4. **ClassicPlugin**：发现、`plugin.txt` 与编码、进程的启动与取消、读回与合并。插件中库一级的部分（发现、合并）在 `tests/auto/plugins/ClassicPlugin` 中测试，以一个测试用的 UTAU 插件（脚本或小程序）端到端运行。
5. **应用**：「工具 → 插件」子菜单、模态对话框、应用结果。
6. **验收**：作者以社区常用的原版插件试用。

## 作者的决定（2026-09-30）

- stdutau 中插件一侧的 `PluginFileReader` / `PluginFileWriter` 不用于宿主。
- **`shell=use`**：Windows 上照 UTAU 用 `ShellExecuteEx`。处理程序不返回进程句柄时（html、hta 可能交给已打开的程序）无法等待，提示用户在插件完成后手动确认。AGENTS.md 中的「尚未确定」随之解决。
- **非 Windows 平台上的 `.exe` 插件**：第一版在菜单中显示但不可用，并说明原因。Wine 以后再议。
- **发现**：设置中 UTAU 文件夹下的 `plugins`（一层，同 UTAU），另加 HelloUtau 自己的用户目录（与原生插件的目录分开）。启动时与「刷新」时重新扫描。
- **取消**：「取消」结束插件的整个进程树并丢弃结果，不设超时。
- **退出码**：不检查，只看文件。
- **协议代码放在 stdutau**，与 `PluginTxt`、插件一侧的 `PluginFileReader` / `PluginFileWriter` 同处。stdutau 现有的是协议的插件一半（读输入、写结果），宿主要的是另一半，须新增两个类：
  - 输入的写出：由设置、前后音符与选区写出 `[#VERSION]`、`[#SETTING]`、`[#PREV]`、带 `@` 条目的编号音符、`[#NEXT]`，CRLF；
  - 结果的读取：把插件写回的文件解析为按顺序的段落（编号音符、`[#INSERT]`、`[#DELETE]`、`[#PREV]`/`[#NEXT]`），并记录每段出现过的条目，供宿主逐条合并。
  - `PluginFileWriter` 的输出正是结果读取的输入，可作测试的对照。
- **首次运行的确认**：新发现的插件第一次运行前弹出一次确认（插件名与将运行的程序），确认过的记入设置，不再询问；插件的程序改变后重新询问。
