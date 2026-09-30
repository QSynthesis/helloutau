# 开发计划

本文档规定工作内容、实施顺序以及每一步的验收标准。开始工作前应先阅读 [`note.md`](note.md)（产品形态）和 [`Status.md`](Status.md)（当前进度）。

## 「与 UTAU 一致」的定义

这一要求若不划定范围，每项决定都会引发争议。范围如下：

**必须一致的是数据和协议。** 包括文件格式（`.ust`、`oto.ini`、`prefix.map`、`character.txt`、`.uar`）、引擎的命令行约定、插件的临时文件协议，以及**同一工程渲染出的音频**。这些是用户的资产和生态，改动它们等于另起炉灶。

**无需一致的是软件本身。** 包括界面、交互、快捷键、性能、内部结构和并发能力。UTAU 在这些方面的表现是一个 2013 年的 VB6 程序的水平，没有沿用的理由。

**判据必须可机械执行。** 「与 UTAU 一致」最严格的检验是：同一工程分别用 UTAU 和 HelloUtau 渲染，比较输出的 wav。两者使用相同的引擎（`resampler.exe` / `wavtool.exe`），因此差异只可能来自参数计算。该比较应实现为可重复执行的测试，而非依赖人工判断。

## 从 QSynthesis 沿用的内容

QSynthesis 是同一作者已停止维护的前作，副本位于 `.cache/QSynthesis-Old`。它**只能作为行为参考，不能作为代码来源**：Qt 5、整套 `Q` 前缀类型、自行实现的 `MiniSystem` 基础设施都不应迁移过来。

值得沿用的是它解决过的**问题**，而非它的代码：

| 功能 | 是否沿用 | 理由 |
|---|---|---|
| 多线程渲染调度（`RealtimeRenderer` + `ResampleWork` + `ConcatenateWork`） | **沿用** | 纯体验提升，UTAU 的单线程渲染是其自身的限制，而非格式的限制 |
| 包络、Mode2 音高、颤音的控制点可视化编辑 | **沿用** | 同上，且这是 UTAU 最难用的部分 |
| 按关注点拆分的撤销操作（`Operations/Notes`、`Operations/Tracks`） | **沿用结构** | 撤销栈的划分方式正确，代码重写 |
| 自定义配色、快捷键编辑器、多语言 | **沿用** | 不影响数据 |
| MIDI 导入 | **沿用，但重写** | **优先级最高**，见 [`Interchange.md`](Interchange.md)。该实现中有五处属于缺陷而非行为特征，照搬会连同缺陷一并继承 |
| VSQ / SynthV / presamp / frq 导入 | **沿用，但靠后** | 属于兼容周边，不影响主干 |
| **多轨** | **暂不沿用，见下文** | UST 为单轨（`Tracks=1`） |
| `MiniSystem` 的文件监视与路径树 | **不沿用** | 文件监视由 `FileSystemWatcher` 和 `hello-fswatcher` 实现 |
| 将编码问题推迟到界面层处理 | **不沿用** | 本仓库在 I/O 边界解决编码问题，见 `AGENTS.md` |

### 多轨设计

**第一版为单轨，但 `.usth` 的顶层从一开始就是 `tracks` 数组，其长度必须为 1。** 格式规格见 [`UsthFormat.md`](UsthFormat.md)。

这样，开放多轨时无需修改 `version`，旧文件也不会失效。读取方遇到长度不为 1 的文件必须明确报错，不能静默地只取第一项，因为静默丢失数据比无法打开更严重。

正式支持多轨时，导出 `.ust` 采用每轨一个文件的方式，UST 互转的保证不受影响。

## 阶段

阶段按依赖顺序排列，而非按重要性排列。每个阶段的验收标准都是可执行的检验，而非主观判断。

### 一、数据层

不涉及界面。本阶段完成后 HelloUtau 尚不可用，但已能正确读取和写出 UTAU 的全部数据。

- `HelloKitSupport`：各模块共用、不属于具体业务的基础设施。编码策略的实现：编码的记录与读取（`_USTH_`、`hello-config.json`、`plugin.json`）、转义与还原、非 Windows 平台上的 ANSI 代码页转换（winacp）。磁盘变化提示：`FileSystemWatcher` 及其启动的 `hello-fswatcher`。
- `HelloKitDocument`：`_USTH_` 控制音符的读写，`.usth` 与 `.ust` 的双向转换，控制音符写入与读取的一一对应。`PayloadCodec` 已完成。
- `HelloKitVoiceBank`：音源目录模型。一个音源可含多份 `oto.ini`，各自使用独立的编码；另有 `prefix.map`、`character.txt`、`readme.txt`。
- `HelloKitInterchange`：MIDI 导入。结构与约束见 [`Interchange.md`](Interchange.md)。

**验收标准**：命令行工具读入 `.ust` 后写回，用 stdutau 解析两侧并规范化后语义相同；任意字节的转义往返测试通过；真实音源（含日文与中文文件名）的目录扫描成功；一个 MIDI 文件在无界面环境下导入为 `.usth`。

### MIDI 导入的阶段安排

MIDI 导入属于「兼容周边」类别，但优先级被提至最高，并且**无需等待界面**：`InterchangeSelector` 将用户决策抽象为回调，内置的 `AutomaticSelector` 使命令行和测试能够执行完整的导入流程。界面上的轨道选择对话框和四种插入位置在第三阶段补充。

### 二、合成

仍不涉及界面。本阶段完成后 HelloUtau 能够输出音频。

- `HelloKitSynth`：用 `utau::Synth::calc` 计算参数，用 `stdc::Popen` 启动引擎，**以参数数组传递，不拼接命令行**，见 `AGENTS.md` 的安全底线。
- 多线程调度，结构参考 QSynthesis 的 `Frontend/Process/`。
- 缓存管理，`CacheDir` 随工程而定（这是 UTAU 的实际行为，已经实测）。

**验收标准**：命令行能将 `.ust` 渲染为 wav；**与 UTAU 渲染同一工程的输出进行比较**。这是整个项目中对「与 UTAU 一致」最严格的检验，值得为其构建一套可重复执行的比较装置。

### 三、编辑器骨架

**第一步是 `HelloKitEdit`，先于窗口和卷帘**，见 [`Editing.md`](Editing.md)。编辑作用于一棵节点树，修改树的唯一途径是会话上的一组类型化函数，由这些函数自行记录变更。界面因此只负责将鼠标操作转换为函数调用，撤销栈也不是事后补充的。另一项直接收益是：**编辑语义在没有界面时即可测试**。

- `HelloKitEdit`：节点树、类型化的修改函数、事务与撤销、命令的文本接口、无界面模式。
- `HelloUtauEditor`：钢琴卷帘、音符的增删改移、歌词输入、工程的打开与保存。
- 渲染与播放接入第二阶段的调度。

**验收标准**：能够打开一个真实工程，修改若干音符，保存后播放，且保存的文件能被 UTAU 正常打开。编辑层自身的验收标准见 [`Editing.md`](Editing.md) 末尾。

### 四、调音

这是 QSynthesis 的强项，也是最应超越 UTAU 的部分。

- 包络、Mode2 音高曲线、颤音的控制点编辑。
- 参数的批量操作与曲线绘制。

**验收标准**：三类参数均可可视化编辑并正确写回 UST，修改后的文件在 UTAU 中打开时曲线相同。结构、步骤与作者确定的交互见 [`Tuning.md`](Tuning.md)。

### 五、插件

六类插件，定义见 `docs/note.md`。

- 原版 UTAU 插件（含 `.bat`，通过 `stdc::Popen::shell(true)` 执行）。
- 内置的 `RangeEditPlugin`、`VoiceBankPlugin`（`hellokit`）。
- `EditorExtensionPlugin`（`helloutau/plugins/`）。

**验收标准**：若干社区常用的原版插件能够正常执行并写回结果；接口为纯虚类，插件不反向链接应用。

格式转换插件（`InterchangePlugin`）由 `HelloKitInterchange` 定义，见 [`Interchange.md`](Interchange.md)。

### 六、兼容周边

- 原音设定编辑器（`oto.ini` 的可视化编辑）。UTAU 自带此功能，用户依赖程度高。作为独立的音源窗口实现，结构与步骤见 [`VoiceBankEditor.md`](VoiceBankEditor.md)。
- 频率表（frq、mrq、pmk 等）的读取、生成与编辑。参考实现 qfrqeditor 已完成，见 [`claude/frqeditor-reference.md`](claude/frqeditor-reference.md)。
- VSQ / SynthV / `.ustx` 导入，MIDI 导出。经由 `HelloKitInterchange` 实现，见 [`Interchange.md`](Interchange.md)。MIDI 导入已在第一阶段完成。
- `.uar` 音源安装，**条目路径按安全底线处理**。

## 贯穿各阶段的要求

以下三项不属于任何单一阶段，每个阶段都必须兼顾：

**编码。** 这是本仓库最容易出错的部分。规则只有一条，见 `AGENTS.md`：`std::string` 一律为 UTF-8，原始字节不得离开 I/O 边界层。

**安全。** 官方 UTAU 的两个 CVE 对应两条安全底线，见 `AGENTS.md`。畸形输入必须有专门的测试，没有测试即视为未实现。

**可测试性。** 核心逻辑位于不依赖 QtWidgets 的层中，应用侧逻辑位于库中而非可执行文件中。这是模块划分方式的依据。
