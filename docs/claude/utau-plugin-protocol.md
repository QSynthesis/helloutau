# UTAU 插件协议的实测

本文档记录实测得到的 UTAU 对原版插件的行为：写给插件的临时文件的内容、位置与名称，以及 UTAU 怎样应用插件的结果。结论来自 2026-09-30 在 Windows（中文系统，ANSI 代码页 936）上对 `utau.exe`（文件版本 0.04.0018）的实测，探针与操作步骤位于 `.cache/utau-probe/plugin/`（`README.md`），可重复执行。计划见 [`../ClassicPluginHost.md`](../ClassicPluginHost.md)。

探针工程 `probe.ust` 有七个音符 **R a i R u e o**（轨道位置 0 至 6），带音高、包络、颤音、flags、velocity、`Label` 与用户条目 `$probe`。

## 临时文件

- **位置与名称**：`%TEMP%\utau1\tmpXXXX.tmp`，如 `C:\Users\user\AppData\Local\Temp\utau1\tmp4F35.tmp`、`tmp7CC.tmp`。`tmp` 加一至四位十六进制数，形同 Windows 的 `GetTempFileName`，每次运行不同。
- **编码与换行**：本地编码，即系统 ANSI 代码页，CRLF。中文系统上 `@alias` 的非 ASCII 字符按 936 写出。规格写作 Shift_JIS，只因规格作者使用日文系统。HelloUtau 另为在 `plugin.json` 中声明接收 UTF-8 的插件写出 UTF-8，见 [`../note.md`](../note.md)「插件」。
- **开头**：`[#VERSION]` 为 `UST Version 1.20`（与工程文件的 `UST Version1.2` 不同）。`[#SETTING]` 依次为 `Project`（工程文件的绝对路径）、`Tempo`、`VoiceDir`、`CacheDir`（均为绝对路径）、`Mode2`，与 [`utau-voicedir-cachedir.md`](utau-voicedir-cachedir.md) 一致。
- **编号**：**编号段落以音符在轨道中的位置编号**。选中 i、R、u 时为 `[#0002]`、`[#0003]`、`[#0004]`，不从 0 起。
- **`[#PREV]` / `[#NEXT]`**：选区前后各一个音符，写成完整的音符，带全部条目与 `@` 条目，也带 `Label` 与 `$probe`。选区从第一个音符开始时没有 `[#PREV]`，到最后一个音符结束时没有 `[#NEXT]`。没有 `[#TRACKEND]`。
- **`notes=all`**：只选 i、R 时仍传入全部七个音符 `[#0000]` 至 `[#0006]`，没有 `[#PREV]` / `[#NEXT]`。选区不以任何方式体现。
- **音符的条目**：依次为 `Length`、`Lyric`、`NoteNum`、`PreUtterance=`（空值，总是写出），然后是音符有的 `Velocity`、`Intensity`、`Modulation`、`PBS`/`PBW`/`PBY`、`Envelope`、`VBR`、`Flags`、`Label`、用户条目，最后是只读的 `@preuttr`、`@overlap`、`@stpoint`，以及非休止符的 `@filename`。
  - `@alias` 只在实际使用的别名与歌词不同时写出（a 经音源映射为另一别名时写出，i、u、e、o 没有）。
  - 休止符只有 `@preuttr=0`、`@overlap=0`、`@stpoint=0`，没有 `@filename`。
  - 没有出现 `@cache`。

## 结果的应用

| 结果 | 保存后的工程 | 结论 |
|---|---|---|
| `[#0004] Lyric=X`、`[#0003]`、`[#0002] Lyric=Z`（编号写反） | i 变为 X，u 变为 Z，R 不变 | **按出现顺序应用，不看编号**；只写段落头的音符不变 |
| `[#PREV] Lyric=P` | a 变为 P，其音高保留 | `[#PREV]` 的修改被应用，省略的条目不变 |
| `[#INSERT] Lyric=N`，写在第一个编号段落之前 | 在 i 之前插入 N，`Length=480`、`NoteNum=62`，没有别的条目 | 插入的长度与音高取自后一个音符（i），与规格一致 |
| `[#0002]`（段落头） | i 不变，包络与颤音保留 | 同上 |
| `[#DELETE]`（第二个编号段落的位置） | 休止符 R 被删除 | 删除该位置的音符 |
| `[#0004] Velocity=`、`Length=240`，省略 `Flags` | u 的 `Velocity` 消失（回到默认），`Length=240`，`Flags=g-5` 保留 | 空值恢复默认，省略即不变 |
| `[#NEXT] Lyric=Q` | e 变为 Q，`Label` 与 `$probe` 保留 | `[#NEXT]` 的修改被应用 |
| 空文件 | 工程不变 | 全部段落省略即取消 |
| `[#INSERT] Lyric=B` 写在 `[#PREV]`（段落头）之前，`[#INSERT] Lyric=F` 写在 `[#NEXT]`（段落头）之后 | B 在 a 与 i 之间，F 在 u 与 e 之间，a 与 e 不变 | **插入不会越出选区**：写在 PREV 之前等于插在选区起点，写在 NEXT 之后等于插在选区终点 |

最后一行是 R4，与前三次分开补做，从 `probe.ust` 重新打开，选区同为 i、R、u；其结果文件的写法与 stdutau `PluginFileWriter::prependNotesBeforePrev` / `appendNotesAfterNext` 的输出相同。

运行时 UTAU 没有弹出提示。前三次应用均可撤销（作者先另存、再撤销、再做下一步：R2、R3 的临时文件中 `Project` 分别为上一步另存的文件，而音符是撤销后的原样）。

## 对 HelloUtau 的影响

- 临时文件按上述写法生成：轨道位置编号，完整的 `[#PREV]` / `[#NEXT]`，`UST Version 1.20`，绝对路径，CRLF，`PreUtterance=` 空值，`@` 条目在末尾。`notes` 存在时传入全部音符，不写 `[#PREV]` / `[#NEXT]`。
- 结果按出现顺序应用，`[#PREV]` / `[#NEXT]` 的修改应用，逐条合并（省略不变、空值恢复默认），`[#INSERT]` 的默认值取自后一个音符，空文件为取消。与 [`../ClassicPluginHost.md`](../ClassicPluginHost.md) 的设计一致。
- stdutau 的 `PluginFileReader`（现并入 `PluginInput`）注释称临时文件「总从 0 编号」，与实测不符；`PluginFileWriter`（现并入 `PluginResult`）的 `prependNotesBeforePrev` / `appendNotesAfterNext` 称插到选区外，实测只插在选区的起点与终点。两处已在 stdutau `ae46c5c` 改正。
- 临时文件的编码是本地编码，与 note.md 对没有 `plugin.json` 的插件的规定一致；声明接收 UTF-8 的插件改用 UTF-8。
