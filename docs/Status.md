# 项目状态

## 现有内容

仓库骨架按 synthrt 的结构搭建，两个模块均可构建。

| 目标 | 状态 |
|---|---|
| `HelloKitSupport` | `Diagnostic`、`TextCodec`（编码名解析、转义与还原）、`FileSystemWatcher`（磁盘变化提示，由 `hello-fswatcher` 进程实现） |
| `HelloKitDocument` | `PayloadCodec`、`Project` / `Track` / `Note` 模型、`.usth` 读写、`UstDocument` |
| `HelloKitVoiceBank` | `VoiceBankConfig`、`VoiceBankSource`（原始扫描）、`VoiceBank`（解码后的模型、查询、写回、与磁盘核对）、`VoiceBankCheckScheduler` |
| `HelloKitSynth` | `EngineProcess`、`SynthPlan`（仅计算）、`SynthRunner` 及其实现 `ClassicSynthRunner`、`ThreadedSynthRunner` |
| `HelloKitInterchange` | 接口、注册表、`Formats/MidiConvert`（导入与导出） |
| `HelloUtauWidgets` | 仅含一个 `QLabel` 的 `MainWindow`，用于验证 Qt Widgets 与 moc 的集成 |
| `helloutau` | 薄驱动，仅含 `main.cpp` |

已验证的构建链：qmsetup 的 `hellokit_add_library` / `helloutau_add_library` / `helloutau_add_application`、Qt 6.11 与 AUTOMOC、stdcorelib、stdutau、wolf-midi、QtTest 与 `add_auto_test`、ctest。

`HelloKitSupport` 包含 `Diagnostic` 和 `TextCodec`。编码相关的全部逻辑集中在 `TextCodec`：编码名的解析、无效字节的拒绝、不可表示字符的转义。`HelloKitDocument` 包含 `PayloadCodec`、`Project` / `Track` / `Note` 数据模型和 `.usth` 读写。**`.usth` 的读写属于 `Project`**，因为 `.usth` 不是众多格式之一，而是工程本身的序列化形式；其他格式均经由 Interchange 转换为 `Project`。`.ust` 由 `UstDocument` 表示，即一份**已读入但尚未解码**的 UST：`open()` 只解析一次，编码探测与 `toProject()` 均使用该次解析结果。`HelloKitInterchange` 的接口与注册表已完成（`InterchangeReader` / `InterchangeWriter` / `InterchangeSource` / `InterchangeSelector` / `AutomaticSelector` / `InterchangeRegistry` / `InterchangePlugin`），第一个格式 `Formats/MidiConvert` 支持导入与导出，可在无界面环境下运行和测试。结构与约束见 [`Interchange.md`](Interchange.md)。**界面尚未实现**：轨道选择页和编码选择页属于第三阶段。

`HelloKitVoiceBank` 是音源的目录模型。读取与解码分为两步，理由与 `.ust` 相同：编码选择界面必须先向用户展示原始字节，而读取本身不能依赖编码。`VoiceBankSource` 遍历目录树，收集每一级目录的 `oto.ini` / `prefix.map` / `character.txt` / `readme.txt` / `hello-config.json` 和音频文件名，**不做任何解码，也不写入任何文件**：记录编码意味着向用户的音源目录写入文件，扫描阶段不应做此决定。`VoiceBank` 通过 `VoiceBankCharsetSelector`（无界面环境使用 `FixedCharsetSelector`）逐目录获取编码并解码，`find(noteNum, lyric)` 按「prefix.map → 别名 → 文件名」的顺序返回样本及其时间参数。

以下行为已经实测确认：**编码无法确定的目录只丢弃需要解码的部分**，其样本仍可按文件名演唱，因为文件名无需编码，而没有 `oto.ini` 的音源本来就以这种方式演唱；**文件名本身也是别名**，[官方页面](https://w.atwiki.jp/utaou/pages/106.html)说明 UTAU 将 wav 文件名作为别名读取，音源作者以 `_` 前缀排除不希望被演唱的文件；扫描设有深度和目录数上限，并且不跟随任何符号链接，因为音源是用户选择的文件夹，其结构不可信任。

**音源写回已经实现。** `VoiceBank` 按目录保存，每个目录保留读取时使用的编码。`save()` 只写入有变化的文件，以原编码写入，拒绝编码无法表示的字符，拒绝磁盘上已被其他程序修改的文件，并在全部检查通过后才开始写入。**一份含 903 个条目的 GBK 真实音源原样打开并保存后，`oto.ini` 逐字节相同；修改一个 offset 只改变一行。** 编码设置分为两种：转换（`setDirectory()` 更改编码后保存，字节改变而文字不变）和重新解读（`reread()`，字节不变而文字改变）。转换前，`VoiceBank::isCharsetReadableByUtau()` 判断原版 UTAU 在本机能否正确读取。文件名一律按音源自身的编码解码后再拼接路径，不经过系统代码页；否则，编码与系统代码页不一致的音源会得到错误的路径，含 emoji 的文件名会使打开操作抛出异常。详见 [`Editing.md`](Editing.md) 的「音源是第二种文档」一节。

**音源编辑界面打开期间，磁盘上的任何变化都会被检测到。** 监视由独立进程 `hello-fswatcher` 执行。在 Windows 上，它按 JetBrains 的做法只持有**驱动器根目录**的一个句柄，因此音源中的任何目录（包括音源根目录）都可以删除或重命名；进程崩溃后会重启，并在重启后进行全量核对；Debug 构建中也不会弹出阻塞的对话框。监视结果仅作为提示：`VoiceBank::checkDisk()` 使用目录指纹进行核对（只列目录、不读文件，对修改时间过于接近取指纹时刻的文件比较内容），**只检测、不修改**，检测到的变化在 `reloadFromDisk()` 之前每次都会重复报告。`VoiceBankCheckScheduler` 整合了监视提示、定时全量核对、监视失效后的轮询和手动触发；`reloadAllFromDisk()` 忽略指纹，重新读取全部内容。三个平台均有后端实现：Windows 使用 `ReadDirectoryChangesW`，macOS 使用 FSEvents（逐文件事件，不持有任何句柄），Linux 使用 inotify（每个目录单独注册；新目录先注册监视再报告整棵子树；根目录的每一级上级目录也受监视，以便在上级目录重命名时检测到根目录消失）。`hello-fswatcher` 另有 Python 编写的协议测试 `test_fswatcher`（通过 ctest 运行，需要 `Python3`；在 Windows 上用 `Python3_EXECUTABLE` 避开应用商店的占位 `python`）：两个音源并列，覆盖 15 种操作，已在三个平台上运行。在 Linux（Ubuntu 22.04、GCC 11.4、Qt 6.11.2）与 macOS（macOS 26.6.2、arm64、Apple Clang、Qt 6.10.1）上均已完成完整构建，21 项测试全部通过。GB18030 不是 ANSI 代码页，winacp 不提供，在 Windows 上由代码页函数转换，在其他系统上只有 Qt 带 ICU 时可用。macOS 版 Qt 不带 ICU，因此 GB18030 在 macOS 上不可用，`TextCodec` 将其报告为无效编码。

`HelloKitSynth` 已能输出音频，分为三层。`EngineProcess` 启动引擎，**参数以向量传递，不提供接受完整命令行的重载**，这是 CVE-2024-28886 相关安全底线在代码中的体现。`SynthPlan` 只计算不执行，将 `VoiceBank::find` 与 `utau::Synth::calc` 衔接，为每个音符生成两条已解析的参数向量。`SynthRunner` 执行计划，现有两种实现：`ClassicSynthRunner` 写出并执行 UTAU 式的渲染脚本，`ThreadedSynthRunner` 以多线程执行重采样器调用。在这一层中，「原始字节」即 UTF-8：`EngineProcess` 接收 UTF-8，工程本身已是文本，整个过程不涉及转码。

**`wavtool.exe` 不直接写出 wav 文件。** 它向 `<out>.whd`（44 字节文件头）和 `<out>.dat`（PCM 数据）追加内容，二者拼接后才是 wav 文件；官方 UTAU 批处理文件末尾的 `copy /B` 即执行此拼接。渲染开始前必须清除这两个残留文件，否则第二次渲染的输出会追加在第一次之后。

运行器的测试通过 `SynthRunner::makeEngineProcess()` 这一测试接缝注入替身引擎，覆盖参数交付之后的行为，见 [`test_ThreadedSynthRunner.cpp`](../hellokit/tests/auto/Synth/test_ThreadedSynthRunner.cpp) 的文件头注释。

`hellokit/tests/manual/ustrender/` 使用真实音源和真实引擎进行渲染，`--plan` 只打印参数而不执行任何程序。引擎路径必须显式指定，工程中的 `Tool1` / `Tool2` 一律不使用。

`hellokit/tests/manual/ustconv/` 是手动运行的命令行工具，用于集成上述各部分：`.ust` / `.usth` / `.mid` 三种格式两两互转，参数解析使用 `stdc::cli`。它不纳入 ctest，其作用是验证各部分组合后能否正常工作，这是各自的自动测试无法覆盖的。

`ustconv --check <file.ust>` 对应路线图第一阶段「读入后写回，两侧语义相同」这一标准：读取一份 UST，写回临时文件，再读取该文件，用 stdutau 的两次解析结果逐字段比较；如有不一致，报告具体音符、字段及两侧的值，并以非零退出码结束。

比较的对象是**值**而非字节：数字的书写形式由写出方决定；文本先按各自的规则解码再比较，因为两份文件可能编码不同、转义规则也不同（只有含控制音符的文件使用转义），这些都不构成工程上的差异。控制音符在两侧均被跳过，因为它属于设计的一部分，而非数据丢失。

`PayloadCodec` 实现 `_USTH_` 控制音符的载荷编码，即去除填充的 base64url。将其作为第一块代码，并非因为它最重要，而是因为它是纯逻辑、不依赖 Qt，且规则已经实测确定（见 [`claude/utau-ust-preservation.md`](claude/utau-ust-preservation.md)）。

## 依赖来源

- **stdcorelib、stdutau**：**均不取自 vcpkg，也均不作为子模块**，二者都与本仓库同步开发。分别构建并安装，配置时传入 `-Dstdcorelib_DIR=` 和 `-Dstdutau_DIR=`，指向 `<prefix>/lib/cmake/<名称>`。`third-party/Dependencies.cmake` 统一执行 `find_package`，由根目录的 `CMakeLists.txt` 通过 `include()` 引入。在 Windows 上，该文件还会将动态库复制到运行输出目录，vcpkg 的 applocal 不再负责这两个库。stdutau 目前是静态库，因此没有需要复制的 DLL。
- **stdcorelib 仅作为私有依赖**：子库使用 `LINKS_PRIVATE`，公开头文件中的导出宏使用 `<QtCore/QtGlobal>` 的 `Q_DECL_EXPORT` / `Q_DECL_IMPORT`。
- **winacp**：Windows 全部 ANSI 代码页的转换表，由 Windows 的 `MultiByteToWideChar` / `WideCharToMultiByte` 生成，在三个平台上逐字节一致。`TextCodec` 的 Shift_JIS、GBK、Big5、EUC-KR 以及 `windows-874`、`windows-1250`–`1258` 均由其转换。需自行构建安装，配置时传入 `-Dwinacp_DIR=`。采用它的原因是 macOS 版 Qt 不包含 ICU，原有实现在 macOS 上无法打开任何 Shift_JIS 文件。
- **wolf-midi**：MIDI 的解析与写出，是去除 Qt 依赖的 `QMidiFile`。来自 `E:/GitHub/ds-editor-lite/vcpkg`，同样通过 `-Dwolf-midi_DIR=` 指定。其 `MidiFile.cpp` 使用 `std::log2` 却未包含 `<cmath>`，GCC 下须以 `-DCMAKE_CXX_FLAGS="-include cmath"` 构建。
- **qmsetup**：来自 `D:/GitHub/synthrt/vcpkg`。
- **Qt 6.11.1**：`D:/Qt/6.11.1/msvc2022_64`。

上述路径均记录在 `.vscode/settings.json` 中，该文件已被 gitignore。

## 后续工作

阶段划分、各阶段的验收标准以及从 QSynthesis 沿用的内容，均见 [`Roadmap.md`](Roadmap.md)。**第一阶段「数据层」的四项验收标准均已达成**：转义往返、MIDI 无界面导入、UST 读写语义一致（`ustconv --check`）、真实音源的目录扫描（一份 GBK 编码的中文音源，903 个 oto 条目，`character.txt` 中的作者名和 `Version:1.0` 等非条目行均未丢失）。

**第二阶段「合成」的两项验收标准也已达成。** 第一项「命令行能将 `.ust` 渲染为 wav」：使用真实引擎和真实音源（GBK 编码的中文音源），输出为合法的 44.1 kHz 单声道 16 位 wav 文件。第二项「与 UTAU 渲染同一工程并比较」：所用装置为 `tests/manual/utauprobe` 和 `tests/manual/utaucompare`，在作者亲自调校的一首歌曲上，引擎参数逐项一致，音高曲线 8673 个值的中位偏差为 0，最大偏差为 15 音分。具体数据与判据见 [`Synth.md`](Synth.md)。

缓存管理也已完成：已渲染的音频片段不会重复渲染；缓存文件名是其内容的摘要，因此修改过的音符会自动得到新文件名并重新渲染。见 [`Synth.md`](Synth.md) 的「缓存」一节。

## 插件位置

按归属划分，而非按类型划分：

| 插件 | 归属 | 理由 |
|---|---|---|
| `RangeEditPlugin`、`VoiceBankPlugin` | `hellokit` | 只处理数据，不涉及界面 |
| `EditorExtensionPlugin` | `helloutau/plugins/` | 涉及界面，归属应用一侧 |

**由此 `hellokit` 完全不需要链接 QtWidgets**，「核心不依赖 GUI」与「插件可扩展界面」两项要求不再冲突。

插件是运行时加载的 MODULE 库，只依赖接口头文件，不反向链接应用，因此应用无需拆分为「共享库加薄驱动」。此结论成立的前提是**接口必须是纯虚类，且不含非内联符号**，编写接口时必须遵守。

## 待定事项

- stdutau 转为子模块的时机。
- 是否安装 `hellokit` 并提供给插件作者。目前 `HELLOKIT_DEVEL` 为 ON，头文件和 CMake 包均会安装。
- 是否将 `EditorExtensionPlugin` 的接口头文件安装并提供给第三方。若提供，需将 `HELLOUTAU_DEVEL` 由 OFF 改为 ON。

## 已知问题

均记录在 [`../AGENTS.md`](../AGENTS.md) 的「已知问题」一节，开始工作前应先阅读。
