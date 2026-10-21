# HelloUtau 使用手册

HelloUtau 是一款跨平台的第三方 UTAU 编辑器，在功能上尽可能与官方 UTAU 保持一致，并在此基础上改善用户体验与扩展性。

## HelloUtau 工程文件

扩展名：`.usth`（完整规格见 [`UsthFormat.md`](UsthFormat.md)）
格式：`json`
编码：UTF-8
格式版本：1

第一版为单轨，UST 本身也是单轨格式（`Tracks=1`）。但顶层从一开始就是 `tracks` 数组，将来开放多轨时无需修改格式版本。

可与 `.ust` 文件相互转换。

### 控制音符

由 `.usth` 生成的 UST 中，第一个音符是 HelloUtau 保留的音符，歌词固定为 `_USTH_`。它通过一个名为 `$usth` 的用户定义条目存放 HelloUtau 的配置信息。

该歌词的命名是为了醒目：用户在 UTAU 中能立即识别出这个音符并非自己所写。**若要保持与 HelloUtau 的兼容，请勿删除该音符。**

对 UTAU 而言，这个音符不发声：没有任何音源包含该歌词，因此找不到对应的采样，但按 `Length` 占据时长。它**不等同于休止符**（2026-09-29 实测）：UTAU 可以对它调用重采样器，只是没有样本可合成，因此不产生音频，占用重采样器的时间可以忽略。每次从 `.usth` 导出为 `.ust` 时添加一个，每次从 `.ust` 导入为 `.usth` 时恰好移除一个。

**建议原 UST 的第一个音符以休止符起步。** 原 UST 的第一个音符是有声音符时，UTAU 把控制音符当作它的**前一个有声音符**，因而影响先行发声的修正、第一个控制点的起点与 Mode1 的接续，HelloUtau 读回后已去掉控制音符，两侧结果因此不同（作者 2026-09-29 决定维持现状，见 [`Tuning.md`](Tuning.md) 待决问题 2）。休止符同样参与这项修正，但以休止符起步能尽可能减小控制音符带来的影响。

控制音符记录的是**该 UST 的编码**。

UST 自身的 `Charset` 只有两种取值：空，或 `UTF-8`。它只表示该文件是否标记了 `Charset=UTF-8`，不表示具体的编码名称：一份 Shift_JIS 的 UST 与一份 GBK 的 UST 在 UTAU 中无法区分，两者都按本机的 ANSI 代码页读取。控制音符补充的正是这部分信息。

控制音符还记录工程的拍号（作者 2026-10-10 决定），UST 本身没有拍号。逐工程的插件状态将来也会存放在这里。

### 载体必须是音符的理由

本节结论均来自对 UTAU 0.4.19 的实测，探针文件和比较脚本位于 `.cache/utau-probe/`，完整记录见 [`docs/claude/utau-ust-preservation.md`](claude/utau-ust-preservation.md)。

| 位置 | `$` 开头的未知条目 | 不带 `$` 的未知条目 |
|---|---|---|
| 音符段 `[#NNNN]` | **保留** | 丢弃 |
| `[#SETTING]` | 丢弃 | 丢弃 |
| `[#VERSION]` | 丢弃 | 丢弃 |

音符段是唯一能保留内容的位置，且条目必须带 `$` 前缀。UTAU 自身的 `$direct`、`$patch`、`$region` 就是这一命名空间中的实例。

**绝不能自行创建段落。** 未知段落不会被忽略，而是被 UTAU 当作音符处理：实测中 `[#HUPDATA]` 被转换为一个 `Length=15`、`NoteNum=24`、歌词为空的无效音符，其后所有音符的索引后移一位，且全程没有任何提示。反过来，HelloUtau 读到无法识别的段名时**静默跳过**。

条目顺序不保留，UTAU 会将未知条目移至段尾，因此不得用位置表达任何含义。同名条目重复时后者生效。

### 载荷格式

条目的值采用**去除填充的 base64url**（字母表 `A-Za-z0-9-_`）。这一选择的依据如下：

- `=` 会截断值，因此不能使用带填充的 base64
- 制表符不仅截断值，还会导致本段其后的所有条目一并丢失
- 空格会被替换为逗号
- 值必须是纯 ASCII，因为读取时需要先将文件视为 ASCII，定位到编码声明后，才能解码整个文件
- **值不得为空**：空值会导致整条被删除，读取时将无法区分「条目不存在」与「条目为空」

长度没有实际限制，实测 65536 个字符被原样保留。

### 往返保证

保证分为两个层次，不可混淆。

**HelloUtau 专有信息无损。** 用户在 UTAU 中编辑该 UST 时，只要不改动控制音符，保存出的新 UST 就能完整地转换回 `.usth`。

**工程本身不保证字节级无损，只保证 UTAU 能以等价的方式打开。** UTAU 保存时会规范化数值、省略等于默认值的条目、重新计算 `@preuttr` 系列的只读值。判据是语义等价：两侧各自解析为内存模型，规范化后进行比较。

**路径的分隔符是原样保存的唯一例外**（作者 2026-10-06 决定）。`VoiceDir`、`OutFile`、`CacheDir`、`Tool1`、`Tool2` 保存时，以 `/` 开头的 Unix 绝对路径一律写 `/`，其余路径（带盘符的路径、相对路径、`%VOICE%` 路径）一律写 `\`，与 UTAU 的写法一致，因此同一工程在任何平台上保存的结果相同。除分隔符外不做任何规范化，`..` 原样保留，因为 `%VOICE%` 是文本前缀而非目录，规范化会改变路径的含义。读取时，`\` 在所有平台上都作为分隔符（`Project::pathOf()`）。

**输出文件名按 Windows 的命名规则检查，在所有平台上一致**（作者 2026-10-13 决定，`Project::isValidOutputFile()`），因为工程也会在 UTAU 中打开。路径中的每一级名称不得含 `<>:"|?*` 与控制字符，不得为设备名（`CON`、`PRN`、`AUX`、`NUL`、`COM1`–`COM9`、`LPT1`–`LPT9`，有无扩展名均同），不得以句点或空格结尾。盘符、`.`、`..` 与空值有效。规则出自 [Naming Files, Paths, and Namespaces](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file)「Naming Conventions」。`&`、`%` 等 shell 元字符是合法的文件名字符，不拦截：输出文件名不经 shell，经典方式写入脚本的值加引号，含引号或换行的值整份拒绝。打开工程时，无效的输出文件名给出警告，原样保存，但不使用，导出整轨时按没有输出文件处理。

## HelloUtau 标记文件

音源文件夹：`hello-config.json`
插件文件夹：`plugin.json`

`hello-config.json` 记录同一目录下 UTAU 原版配置文件 `oto.ini`、`prefix.map`、`character.txt` 的编码，以及 HelloUtau 需要在该目录层级保存的其他数据。

**每个文件夹一份，而非每个音源一份。** 一个音源可以在各级子目录中包含多份 `oto.ini`，其编码可能各不相同，因此标记文件按目录存放，与其描述的文件位于同一目录。这样，音源的某个子目录被单独复制时，编码信息也随之保留。

**文件名编码错解的记录**（作者 2026-10-11 决定，2026-10-12 改为三种形式）：`hello-config.json` 以 `fileNameEncoding` 记录用户对该目录文件名的决定。记录只管该目录的音频文件与这一层的子文件夹，ASCII 名字不受影响。没有记录表示尚未决定，打开时询问。记录有三种形式：

```json
"fileNameEncoding": {}
```

```json
"fileNameEncoding": {
  "original": "Shift_JIS",
  "misdecodedAs": "GBK"
}
```

```json
"fileNameEncoding": {
  "original": "Shift_JIS",
  "misdecodedAs": "GBK",
  "files": {
    "misdecoded": ["_偁媧偁揻.wav"],
    "correct": ["_あ長.wav"]
  }
}
```

- **空对象**：名字全部正确，按原样使用，之后新增的名字同样按原样使用。
- **只有两个编码**：名字全部错解，读取时全部还原，之后新增的名字同样还原。`original` 是文件名创建时的编码，`misdecodedAs` 是名字的字节被错误解释时使用的编码。还原失败的名字按原样使用，不报告警告，因为这样的名字不可能是错解的。
- **两个编码加 `files`**：一部分名字错解。`misdecoded` 与 `correct` 分别列出错解的与本来就正确的物理名称，每个非 ASCII 的这类名字都在两个列表之一。两个列表都没有的名字是新增的名字，状态无法推断，须询问用户。

两个编码要么都有，要么都没有。有 `files` 时必须有两个编码，`files` 中两个列表都必须有。记录不符合这些规则、含有其他字段或类型不符时，视为没有记录并给出警告，保存时原样写回。处理规则见 [`Editing.md`](Editing.md)「文件名编码错解的处理」。

`plugin.json` 是插件的完整清单，不仅限于编码。对于支持 HelloUtau 的插件，它取代 `plugin.txt` 成为权威来源。

## HelloUtau 编码兼容策略

### 工程

`.usth` 始终为 UTF-8，无需选择。

打开 `.ust` 时：若为 HelloUtau 写出的文件，直接使用控制音符中记录的编码；否则检查 `Charset=UTF-8`；若仍无法确定，则要求用户选择。确定编码后，整个文件转换为 UTF-8。

导出 `.ust` 时的编码**可以配置**，默认为 UTF-8，每次导出时也可单独更改。默认采用 UTF-8，是因为 Shift_JIS 无法表示中文歌词和中文文件名，转义后的文本既不易读也不便使用；需要将文件交给仍在使用旧插件和旧工具的用户时，可改为 Shift_JIS。无论选择哪种编码，实际使用的编码都记录在控制音符中。

### 音源库

首次打开音源文件夹时要求用户选择编码，此后编码保存在该目录的 `hello-config.json` 中。

### 插件

插件文件夹中没有 `plugin.json` 时，`plugin.txt` 在 Windows 上按系统 ANSI 代码页读取，在其他平台上按 CP932 读取，因为绝大多数 UTAU 插件是日文的，而 ANSI 代码页的概念在 Windows 之外不存在。

有 `plugin.json` 时以它为准，不再读 `plugin.txt`（作者 2026-09-30 定）。它是 UTF-8 的 JSON：

```json
{"name": "插件名", "execute": "plugin.exe", "shell": false, "notes": "selection", "charset": "UTF-8",
 "ustversion": "1.20"}
```

- `name`：省略时取文件夹名。
- `shell`：为 `true` 时同 `plugin.txt` 的 `shell=use`。
- `notes`：`selection`（默认）或 `all`。
- `charset`：传给插件的临时文件的编码，省略时与 UTAU 相同，即上述本地编码。
- `ustversion`：临时文件的条目格式，同 `plugin.txt` 的 `ustversion`，取值 `1.00`、`1.10` 或 `1.20`，省略时为 `1.20`（作者 2026-10-14 定）。
- 其他键原样保留，宿主读写 `plugin.json` 时不删除它们（作者 2026-10-14 定）。
- `hostData` 键归宿主使用，记录宿主写入的状态，与插件作者写的清单键分开（作者 2026-10-15 定）。其下每个原生插件各占一个以插件 id 为名的键，宿主读写时保留其他原生插件的键，某个原生插件的键为空时删去，`hostData` 为空时也删去。ClassicPluginHost 在其下记录试听程序的替换，见 [`PluginPlay.md`](PluginPlay.md)「部署」：

  ```json
  "hostData": {"ClassicPluginHost": {"playinplugin": {"original": "playinplugin-original.exe"}}}
  ```

  `original` 为原有的 `playinplugin.exe` 改名后的文件名，原来没有该文件时为 `null`。移除试听程序时只删去这条记录，`plugin.json` 一律保留，包括安装时由 `plugin.txt` 转写而来的，因为它与 `plugin.txt` 读法相同，且安装后可能被用户或其他原生插件修改过（作者 2026-10-15 定）。

**传给插件的临时文件**（作者 2026-09-30 定）：`plugin.json` 的 `charset` 声明 `UTF-8` 时，临时文件写成 UTF-8，插件的结果也按 UTF-8 读回。否则与 UTAU 兼容，按上述本地编码写出与读回，无法表示的字符按下文转义。UTAU 实际使用本地编码（系统 ANSI 代码页，见 [`claude/utau-plugin-protocol.md`](claude/utau-plugin-protocol.md)）。插件规格写作 Shift_JIS，只因规格作者使用日文系统。

### 文件名的 Unicode 规范化

带浊点的假名等字符有两种 Unicode 写法：组合形式（NFC）以一个字符表示「が」（U+304C），分解形式（NFD）以「か」（U+304B）加组合用浊点（U+3099）表示。两者显示相同，作为字符串比较却不相等。**在 macOS 上，从目录列表读出的文件名一律转换为 NFC**，再进入内存模型或与模型中的名字比较（作者 2026-10-18 决定，`kit::PlatformFileSystem::listedFileName()`），与 Qt 相同：Qt 在 macOS 上写出文件名时转换为 NFD（`QFile::encodeName()`），读入时转换为 NFC（`QFile::decodeName()` 与目录的遍历）。`oto.ini` 等文件解码得到的名字是 NFC，不转换时，同一个文件名在 macOS 上与磁盘上的名字不相等。macOS 的文件系统（APFS 与 HFS+）查找文件时不区分两种写法，以 NFC 的名字打开文件即可找到 NFD 的文件。**其他系统不转换**，因为 Windows 与 Linux 的文件系统区分两种写法，转换后的名字可能找不到文件。复制、移动、删除文件时沿用目录列表的原样路径，不经转换。

### 转义

目标编码无法表示某个字符时，以转义序列表示该字符。目标编码为 UTF-8 时不需要转义，只有在导出为 Shift_JIS 等编码、写入插件临时文件以及写入音源配置时才会发生。

插件临时文件中只转义音符的条目，因为插件会把它们写回、由 HelloUtau 还原。`[#SETTING]` 中的路径与只读的 `@` 条目由插件直接使用，不会写回，因此不转义：转义会把路径中的每个反斜杠加倍，使插件打不开它。其中无法表示的字符与 UTAU 一样丢失。

转义与还原是同一段代码的两个方向，必须成对实现，并且原文中本身出现的转义序列也必须能正确还原。

转义序列采用 JSON 的形式，均为纯 ASCII，因此任何目标编码都能表示：

| 写法 | 含义 |
|---|---|
| `\uXXXX` | 一个 UTF-16 码元，四位小写十六进制数 |
| `\\` | 一个反斜杠 |

基本多文种平面之外的字符写成两个 `\uXXXX`，每个代理项一个，与 JSON 一致。

**转义时所有反斜杠一律加倍，即使本次没有任何字符需要转义。** 若不加倍，将无法区分「原文中本来就有的 `\u0041`」与「转义产生的 `\u0041`」，还原时会将前者错误地还原为 `A`。

**因此转义只适用于 HelloUtau 自己写出、且编码不是 UTF-8 的文件。** UTAU 或其他编辑器写出的 UST 不使用这套规则，对其进行还原会删除其中的反斜杠。判断依据是控制音符：没有控制音符即没有转义；控制音符记录为 UTF-8，说明写出时没有需要转义的字符。

## 安全行为

官方 UTAU 直到 2024 年 5 月发布的 v0.4.19 才修复了存在于此前所有版本中的两个漏洞。HelloUtau 在这两方面都比 UTAU 更严格。

**工程文件中的字符串不可信。** 在官方 UTAU 中打开一份经过构造的 `.ust` 可导致执行任意操作系统命令（CVE-2024-28886，操作系统命令注入）。具体涉及哪个字段、经由哪条路径，官方和 JVN 均未公布。

因此 HelloUtau 在以下两处设防，不依赖于推测具体机制：

- **启动进程一律使用参数数组，绝不拼接成命令行交给 shell。** 官方 UTAU 渲染时默认写出一个批处理文件再执行（v0.4.12 的更新日志中有「wav生成時にバッチを使用しない」这一开关，说明默认使用批处理），而批处理文件是 shell 脚本，从工程中取出的文件名、别名、flags 只要包含 `&` 或换行符，就能追加命令。HelloUtau 不使用批处理。
- **工程自带的合成工具须经信任才执行。** `[#SETTING]` 中的 `Tool1`、`Tool2` 都是写在工程中的路径，直接执行等于让工程指定运行哪个程序。HelloUtau 渲染时一律使用工程的 `Tool1`（合成器）与 `Tool2`（重采样器）。任何播放方式下两者都必须存在，并且是本地设置中的合成工具，或经用户在明确的提示中选择信任，否则不渲染，也不改用本地设置中的合成工具。本地设置中的合成工具用于重置工程的合成工具，以及供音源编辑器试合成（作者 2026-10-06 决定）。信任之后的文件在运行期间保持不变，变化即停止渲染，见 [`ProgramIntegrity.md`](ProgramIntegrity.md)。音符上的 `$patch` 不是合成工具，而是相对于工程文件所在文件夹的 wav 文件名，UTAU 不经重采样器直接拼接（[插件规格](https://w.atwiki.jp/utaou/pages/64.html)），HelloUtau 照 UTAU 渲染（作者 2026-10-09 决定，见 [`Synth.md`](Synth.md)「`$patch` 与 `$direct`」）。**这些设置仍会被原样保存**（分隔符除外，见「往返保证」）：逐工程配置合成工具是 UTAU 的正常用法，丢弃它等于删除用户的设置。危险在于未经询问即执行，而非保存。

**音源与插件压缩包的条目路径不可信。** `.uar` / `.zip` 中的条目名可以是绝对路径或 `../../..`，官方 UTAU 按此路径写入文件，因此可以向任意位置释放文件（CVE-2024-32944）。HelloUtau 遇到绝对路径、盘符、`..` 路径分量或符号链接条目时，拒绝整个压缩包并告知用户，而不是静默过滤。

## 音源文件夹

工程的 `VoiceDir` 按以下规则解析（作者 2026-10-08 决定，`%DATA%` 为 2026-10-18 决定）。UTAU 自身的规则见 [`claude/utau-voicedir-cachedir.md`](claude/utau-voicedir-cachedir.md)：`%VOICE%` 与相对路径都以 `utau.exe` 所在目录为基准，`%DATA%` 表示其安装包所用的 `%AppData%\UTAU`。

- **`%VOICE%` 前缀表示设置中 UTAU 文件夹的 `voice` 目录**，与 UTAU 相同，仅在 UTAU 文件夹存在时有效。
- **`%DATA%` 前缀依次表示以下数据文件夹**，取第一个包含该路径的文件夹：一、HelloUtau 的 `ClassicExtensions`，即文档目录下的 `OpenVPI/HelloUtau/ClassicExtensions`（见 [`Distribution.md`](Distribution.md)「用户目录」），其 `voice` 是 HelloUtau 自己的音源文件夹；二、UTAU 的数据文件夹，即应用数据文件夹下的 `UTAU`：Windows 上为 `%AppData%\UTAU`，UTAU 的安装包把音源装在其 `voice` 中。macOS 上为 `~/Library/Application Support/UTAU`，Linux 上为 `~/.local/share/UTAU`，这两个平台没有原生的 UTAU，该文件夹用于存放从 Windows 复制过来的音源与插件（作者 2026-10-18 决定）。不存在的文件夹视同未列出。`ClassicExtensions` 与 `%AppData%\UTAU` 的结构相同，因此同一个 `%DATA%\voice\名称` 在 HelloUtau 中先找前者，在 UTAU 中找后者。
- **不带前缀的相对路径**：设置中的 UTAU 文件夹存在时，由用户选择按 UTAU 文件夹还是按 HelloUtau 程序所在目录解析，默认按 UTAU 文件夹，与 UTAU 一致；UTAU 文件夹未设置或不存在时，按 HelloUtau 程序所在目录解析。
- **保存时**，位于 UTAU 文件夹的 `voice` 内的绝对路径写为 `%VOICE%名称`，位于某个数据文件夹内的写为 `%DATA%\voice\名称`（前缀后带分隔符，同 UTAU），但仅当该写法解析回同一目录时；被优先级更高的文件夹中的同名文件夹遮蔽时写绝对路径。HelloUtau 音源文件夹中的音源因此写为 `%DATA%\voice\名称`，用 UTAU 打开该工程时到 `%AppData%\UTAU\voice` 中查找，音源不在那里时找不到，这一代价已经作者接受。
- **列出音源与插件**：音源依次列出 `ClassicExtensions/voice`、UTAU 的数据文件夹的 `voice` 与 UTAU 文件夹的 `voice` 中的文件夹，UTAU 插件依次从三处同名的 `plugins` 中查找。

**UTAU 文件夹的用途**：解析以相对路径出现的 `Tool1`、`Tool2`；在上述选项下解析以相对路径出现的音源；其 `voice` 目录即 `%VOICE%` 所指的音源文件夹，其 `plugins` 目录作为 UTAU 插件的来源之一。

## 缓存

与 UTAU 保持一致，缓存目录位于工程文件旁边。UTAU 不使用文件中的 `CacheDir`，缓存目录总是 UST 旁边的 `<文件名去掉扩展名>.cache`，保存与另存为时也将 `CacheDir` 写为这一名称，HelloUtau 采取相同做法。实测见 [`claude/utau-voicedir-cachedir.md`](claude/utau-voicedir-cachedir.md)。

## HelloUtau 插件

HelloUtau 的插件只有一种：**原生插件**，即由 stdcorelib.plugin 载入的 C++ 动态库（作者 2026-09-30 决定，计划见 [`Plugins.md`](Plugins.md)）。HelloUtau 不定义插件类，只提供五个**扩展点**的注册接口，插件在初始化时经这些接口登记它提供的内容：

- 选区编辑：
  - 执行选区修改，执行期间阻塞界面
- 编辑界面扩展：
  - 提供额外的编辑界面，执行期间不阻塞界面
- 音源批量操作：
  - 对音源库执行批量操作，执行期间阻塞界面
- 格式转换：
  - 为 HelloUtau 增加一种可导入或可导出的外部格式，例如 MIDI、VSQ、`.ustx`
  - 登记的是格式驱动而非界面。需要自定义选择步骤界面时另行提供一个页面，接口结构见 [`Interchange.md`](Interchange.md)
- 频率表格式（作者 2026-09-30 决定新增）：
  - 为 HelloUtau 增加一种频率表格式（重采样器为音频文件事先分析的基频），例如某个重采样器自己的分析文件
  - 登记的是格式驱动而非界面，与内置的 frq、dio、mrq 经同一途径加入，格式接口见 [`FrequencyMaps.md`](FrequencyMaps.md)

HelloUtau 内置的功能与插件经同一途径登记，不分主次。

**原版 UTAU 插件**（`plugin.txt` 加可执行文件）不是一种插件，而由随 HelloUtau 提供的一个原生插件支持：它发现 UTAU 插件文件夹，把其中每个 UTAU 插件作为一项命令加入「工具 → Classic Plugins」菜单，运行时修改选区。停用这个原生插件即不再提供 UTAU 插件。UTAU 插件的运行方式：

- 以包含当前选区的 `temp.ust` 作为第一个命令行参数的可执行文件，执行选区修改，执行期间阻塞界面
- 也可以不是 `.exe`。对于 `plugin.txt` 中含 `shell=use` 的插件，官方 UTAU 使用 `ShellExecuteEx` 而非 `CreateProcess` 启动，因此 `.jar`、`.html`、`.hta`、`.bat` 均可运行，实际运行的是系统为该扩展名注册的处理程序。`.bat` 插件须与官方一样显示可见的控制台窗口，使用户能直接看到脚本的输出
