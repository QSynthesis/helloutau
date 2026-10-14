# UTAU 插件协议的实测

本文档记录实测得到的 UTAU 对原版插件的行为：写给插件的临时文件的内容、位置与名称，以及 UTAU 对插件结果的应用。结论来自 2026-09-30 在 Windows（中文系统，ANSI 代码页 936）上对 `utau.exe`（文件版本 0.04.0018）的实测，探针与操作步骤位于 `.cache/utau-probe/plugin/`（`README.md`），可重复执行。计划见 [`../ClassicPluginHost.md`](../ClassicPluginHost.md)。

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
  - 没有出现 `@cache`。PitTrace 的 1.00 样本（工程渲染过，见 [`playinplugin.md`](playinplugin.md)）中有 `@cache`，下一节的探针（工程未渲染）中没有，出现的条件未经实测。

## 条目格式（`ustversion`）

2026-10-14 实测：四个插件只复制临时文件，`plugin.txt` 分别写 `ustversion=1.00`、`1.10`、`1.20` 与不写，作者在同一工程（`.cache/utau-probe/ustversion/1-ustversion.ust`，R a i R u e o，带 Mode1 与 Mode2 音高、带值的 `PreUtterance`、音符内的 `Tempo`、Flags、Label 与用户条目）中对同一选区 i R u 与全选各运行一次。

| | 1.00 | 1.10 | 1.20 |
|---|---|---|---|
| `[#VERSION]` | 无 | 无 | `UST Version 1.20` |
| `[#SETTING]` 的第一项 | `UstVersion=1.00` | `UstVersion=1.10` | `Project` |
| 调制 | `Moduration` | `Moduration` | `Modulation` |
| Mode1 的音高数组 | `Piches` | `Pitches` | `PitchBend` |

- 其余条目、条目的顺序、`[#PREV]` / `[#NEXT]` 与只读条目三种格式相同。条目的顺序固定，与工程中的顺序无关。
- 不写 `ustversion` 时与 1.20 逐字节相同。

## 结果的应用

| 结果 | 保存后的工程 | 结论 |
|---|---|---|
| `[#0004] Lyric=X`、`[#0003]`、`[#0002] Lyric=Z`（编号写反） | i 变为 X，u 变为 Z，R 不变 | **按出现顺序应用，与编号无关**；只写段落头的音符不变 |
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

## 长度修正与曲速

2026-10-14 实测，同一台机器与 `utau.exe`，探针与操作步骤位于 `.cache/utau-probe/length/`（`README.md`、`make_files.py`、`compare.py`），结果文件在其 `results/` 中。动机是作者以前的资料（[Length-Correction](https://sinestriker.github.io/documents/QPitchEditor/book/Developers/UTAU-Backend/Length-Correction.html)）所述：UTAU 写给插件的修正值按当前音符的曲速计算前一个音符的长度，交给合成工具的修正值则按前一个音符自己的曲速计算。

探针工程使用葛平音源，每组的当前音符都是 `qu`（先行发声 186.884，重叠 −5.999，两者之差 192.883 ms），各组只改变前一个音符。对照的是临时文件中 `qu` 的 `@preuttr`，与同一次合成的 `temp.bat` 交给 wavtool 的先行发声（长度参数 `480@120+…` 中的值）：

| 组 | 前一个音符 | 按其自己的曲速的长度 | 按当前音符的曲速的长度 | `@preuttr` | wavtool |
|---|---|---|---|---|---|
| G1 | `a` 240 tick，120 BPM | 250 ms | 250 ms | 121.112 | 121.112 |
| G2 | `a` 240 tick，60 BPM | 500 ms | 250 ms | 121.112 | 186.884 |
| G3 | `a` 240 tick，240 BPM | 125 ms | 250 ms | 121.112 | 60.556 |
| G4 | 休止符 120 tick，120 BPM | 125 ms | 125 ms | 121.112 | 121.112 |
| G5 | 休止符 240 tick，240 BPM | 125 ms | 250 ms | 186.884 | 121.112 |
| G6 | 休止符 120 tick，60 BPM | 250 ms | 125 ms | 121.112 | 186.884 |

修正的上限是前一个音符长度的一半，前一个音符是休止符时为其全部长度。先行发声与重叠之差超过上限时，两者按同一比例缩小，STP 加上先行发声缩小的量。按此计算：

- **临时文件中的值按当前音符的曲速计算前一个音符的长度。** 六组的 `@preuttr`、`@overlap`、`@stpoint` 都与这一算法吻合。G2 与 G6 本不应修正而被修正，G3 修正不足，G5 应修正而未修正。
- **交给 wavtool 的值按前一个音符自己的曲速计算**，六组都吻合。
- **前一个音符是休止符时，临时文件中的值同样经过修正**（G4、G6），PlayInPlugin 旧记录中「前一个音符是休止符时修正值不进入插件」在本次实测中不成立。
- 选区不影响这些值：只选中 G2 的 `qu` 时，其值与全选时相同。
- 只选中 G2 的 `qu` 时，`[#SETTING]` 的 `Tempo` 为 60，即 `[#PREV]`（曲速 60 的 `a`）的曲速，全选时为第一个音符的 120。

HelloUtau 的合成计划（`ustrender --plan`）在六组中与 wavtool 收到的值完全相同。

### `[#SETTING]` 的曲速

上面 `[#PREV]` 自带 `Tempo=60`，不能区分「第一个段落所在的曲速」与「第一个段落的 `Tempo` 条目，没有时为工程的曲速」。同日补做第二轮（`probe-tempo.ust`）：工程的曲速为 60，第 2 个音符改为 120，第 7 个音符改为 90，其余音符没有 `Tempo`。

| 选中 | `[#SETTING]` 的 `Tempo` | `[#PREV]` |
|---|---|---|
| 第 5 个音符 | 120 | 第 4 个音符，没有 `Tempo`，所在的曲速是第 2 个音符设的 120 |
| 第 7 个音符（`Tempo=90`） | 120 | 第 6 个音符，同上 |
| 第 1 个音符 | 60 | 无，`[#NEXT]` 带 `Tempo=120` |

**`Tempo` 是临时文件中第一个段落实际所在的曲速**：有 `[#PREV]` 时为它所在的曲速，没有时为选区第一个音符所在的曲速。之前改过的曲速沿用至此，因此插件由 `[#SETTING]` 与各段落的 `Tempo` 条目即可得知每个音符的曲速。选区以曲速变化开始时，写的是 `[#PREV]` 所在的曲速，不是选区第一个音符的曲速。

## 对 HelloUtau 的影响

- 临时文件按上述写法生成：轨道位置编号，完整的 `[#PREV]` / `[#NEXT]`，`UST Version 1.20`，绝对路径，CRLF，`PreUtterance=` 空值，`@` 条目在末尾。`notes` 存在时传入全部音符，不写 `[#PREV]` / `[#NEXT]`。
- 结果按出现顺序应用，`[#PREV]` / `[#NEXT]` 的修改应用，逐条合并（省略不变、空值恢复默认），`[#INSERT]` 的默认值取自后一个音符，空文件为取消。与 [`../ClassicPluginHost.md`](../ClassicPluginHost.md) 的设计一致。
- stdutau 的 `PluginFileReader`（现并入 `PluginInput`）注释记为临时文件「总从 0 编号」，与实测不符；`PluginFileWriter`（现并入 `PluginResult`）的 `prependNotesBeforePrev` / `appendNotesAfterNext` 注明插到选区外，实测只插在选区的起点与终点。两处已在 stdutau `ae46c5c` 改正。
- 临时文件的编码是本地编码，与 note.md 对没有 `plugin.json` 的插件的规定一致。声明接收 UTF-8 的插件改用 UTF-8。
- `ClassicPluginExchange::input()` 以 `SampleTiming` 写出 `@preuttr`、`@overlap`、`@stpoint`，即交给合成工具的值。前后曲速不同时，它与 UTAU 写给插件的值不同（上表 G2、G3、G5、G6）。作者 2026-10-14 决定不照搬 UTAU 的算法，因为它是错误的。
- `[#SETTING]` 的 `Tempo` 原先取选区第一个音符的曲速，有 `[#PREV]` 且其曲速不同时，插件会把没有 `Tempo` 条目的 `[#PREV]` 按错误的曲速计算。2026-10-14 改为与 UTAU 相同，取第一个段落所在的曲速（`ClassicPluginExchange::input()`）。
