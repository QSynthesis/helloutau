# 开发计划

本文档规定做什么、按什么顺序做、每一步做完怎么算数。做之前先读 [`note.md`](note.md)（产品形态）和 [`Status.md`](Status.md)（当前进度）。

## 什么叫「和 UTAU 一致」

这句话如果不划边界，会变成每个决定都要争一次。边界是这样的：

**必须一致的，是数据和协议。** 文件格式（`.ust`、`oto.ini`、`prefix.map`、`character.txt`、`.uar`）、引擎的命令行契约、插件的临时文件协议，以及**同一个工程渲染出的音频**。这些是用户的资产和生态，改了就等于另起炉灶。

**不必一致的，是软件本身。** 界面、交互、快捷键、性能、内部结构、能同时做几件事。UTAU 在这些方面的样子是 2013 年一个 VB6 程序的样子，没有理由继承。

**判据要机械化。** 「和 UTAU 一致」最硬的检验是：拿同一个工程，分别用 UTAU 和 HelloUTAU 渲染，比较输出的 wav。引擎是同一个（`resampler.exe` / `wavtool.exe`），所以差异只可能来自参数计算。这个比对应当做成可重放的测试，而不是靠肉眼和印象。

## 从 QSynthesis 拿什么

QSynthesis 是同一作者停更的前作，`.cache/QSynthesis-Old` 下有一份。它**只能当行为参考，不能当代码来源**——Qt 5、整套 `Q` 前缀类型、自造的 `MiniSystem` 基础设施，都不该搬过来。

值得拿的是它解决过的**问题**，不是它写下的代码：

| 它做过的 | 拿不拿 | 为什么 |
|---|---|---|
| 多线程渲染调度（`RealtimeRenderer` + `ResampleWork` + `ConcatenateWork`） | **拿** | 纯体验提升，UTAU 单线程渲染是它自己的限制，不是格式的限制 |
| 包络、Mode2 音高、颤音的控制点可视化编辑 | **拿** | 同上，而且这是 UTAU 最难用的地方 |
| 按关注点拆分的撤销操作（`Operations/Notes`、`Operations/Tracks`） | **拿形状** | 撤销栈的分法是对的，代码重写 |
| 自定义配色、快捷键编辑器、多语言 | **拿** | 不影响数据 |
| MIDI 导入 | **拿，但重写** | **优先级最前**，见 [`Interchange.md`](Interchange.md)。那份实现有五处是缺陷不是行为特征，照抄会连缺陷一起继承 |
| VSQ / SynthV / presamp / frq 导入 | **拿，但排在后面** | 兼容周边，不影响主干 |
| **多轨** | **暂不拿，见下** | UST 是单轨（`Tracks=1`） |
| `MiniSystem` 那套自造文件监视与路径树 | **不拿** | 用 Qt 或 stdcorelib 现成的 |
| 把编码问题拖到 UI 层才处理 | **不拿** | 本仓库的编码在 I/O 边界解决，见 `AGENTS.md` |

### 多轨已经定了

**第一版单轨，但 `.usth` 的顶层从一开始就是 `tracks` 数组，长度必须为 1。** 格式规格见 [`UsthFormat.md`](UsthFormat.md)。

这样放开多轨时不必改 `version`，也不必让老文件失效。读取方遇到长度不为 1 的文件要明确报错，不能默默只取第一条——静默丢数据比打不开更糟。

真放开的时候，导出 `.ust` 的办法是一轨一个文件，UST 互转的承诺不受影响。

## 阶段

按依赖顺序，不按重要性。每个阶段的「算数」是可执行的检验，不是感觉。

### 一、数据层

不碰界面。做完这一阶段，HelloUTAU 还不能用，但它已经能正确地读懂和写出 UTAU 的一切。

- `HelloKitSupport`：编码策略的落地。编码的记录与读取（`_USTH_`、`hello-config.json`、`plugin.json`）、转义与还原、ANSI 代码页在非 Windows 上的替身。
- `HelloKitDocument`：`_USTH_` 控制音符的读写，`.usth` ↔ `.ust` 的双向转换，控制音符的一进一出配平。`PayloadCodec` 已完成。
- `HelloKitVoiceBank`：音源目录模型。一个音源多份 `oto.ini`，各自编码；`prefix.map`、`character.txt`、`readme.txt`。
- `HelloKitInterchange`：MIDI 导入。形状与约束见 [`Interchange.md`](Interchange.md)。

**算数**：一个命令行工具把 `.ust` 读进来再写回去，用 stdutau 解析两边、规范化后语义相等；任意字节的转义往返测试绿；拿真实音源（含日文与中文文件名）跑通目录扫描；一个 MIDI 文件在没有界面的情况下导入成 `.usth`。

### MIDI 导入为什么在这一阶段

它在「兼容周边」那一类里，但优先级被提到了最前面，而且**不需要等界面**：`InterchangeSelector` 把提问抽成了回调，内置的 `AutomaticSelector` 让命令行和测试都能跑完整条导入流程。界面上的选轨对话框和四种插入位置第三阶段再补。

### 二、合成

仍不碰界面。做完这一阶段，HelloUTAU 能出声。

- `HelloKitSynth`：用 `utau::Synth::calc` 算参数，`stdc::Popen` 起引擎，**参数数组不拼命令行**，见 `AGENTS.md` 的安全底线。
- 多线程调度，形状参考 QSynthesis 的 `Frontend/Process/`。
- 缓存管理，`CacheDir` 跟工程走（这是 UTAU 的实际行为，已实测）。

**算数**：命令行能把 `.ust` 渲染成 wav；**和 UTAU 渲染同一工程的输出做比对**。这是整个项目里「和 UTAU 一致」最硬的一次检验，值得为它建一套可重放的比对装置。

### 三、编辑器骨架

**第一步是 `HelloKitEdit`，排在窗口和卷帘之前**，见 [`Editing.md`](Editing.md)。编辑发生在一棵节点树上，改树只有一条路——会话上那组类型化的函数，函数自己记账。界面因此只剩「把鼠标动作翻译成函数调用」一件事，撤销栈也不是事后补的。它还带来一个当下的好处：**编辑语义在没有界面的时候就能测**。

- `HelloKitEdit`：节点树、类型化的修改函数、事务与撤销、命令的文本外壳、无头模式。
- `HelloUtauWidgets`：钢琴卷帘、音符的增删改移、歌词输入、工程的开与存。
- 渲染与播放接到第二阶段的调度上。

**算数**：能打开一个真实工程、改几个音符、存回去、播放，存出的文件 UTAU 能正常打开。编辑层自己的算数在 [`Editing.md`](Editing.md) 文末。

### 四、调音

QSynthesis 的强项，也是最该超过 UTAU 的地方。

- 包络、Mode2 音高曲线、颤音的控制点编辑。
- 参数的批量操作与曲线绘制。

**算数**：三类参数都能可视化编辑并正确写回 UST，改完的文件在 UTAU 里打开是同样的曲线。

### 五、插件

四类插件，定义见 `docs/note.md`。

- 原版 UTAU 插件（含 `.bat`，走 `stdc::Popen::shell(true)`）。
- 内置的 `RangeEditPlugin`、`VoiceBankPlugin`（`hellokit`）。
- `EditorExtensionPlugin`（`helloutau/plugins/`）。

**算数**：几个社区常用的原版插件能正常执行并把结果写回；接口是纯虚类，插件不反向链接应用。

还有一类待定：格式转换插件（`InterchangePlugin`）。它会让 note.md 里的四类变成五类，所以要先跟作者确认，见 [`Interchange.md`](Interchange.md) 文末。

### 六、兼容周边

- 原音设定编辑器（`oto.ini` 的可视化编辑）。UTAU 自带，用户依赖度高。
- `frq` 频率表的读取与生成。
- VSQ / SynthV / `.ustx` 导入，MIDI 导出。走 `HelloKitInterchange`，见 [`Interchange.md`](Interchange.md)。MIDI 导入已在第一阶段做掉。
- `.uar` 音源安装，**按安全底线处理条目路径**。

## 贯穿始终的三件事

这三件不属于任何阶段，每个阶段都要顾：

**编码。** 本仓库最容易出错的地方。规则只有一条，见 `AGENTS.md`：`std::string` 一律 UTF-8，原始字节不许离开 I/O 边界那一层。

**安全。** 官方 UTAU 的两个 CVE 对应的两条底线，见 `AGENTS.md`。畸形输入要有专门的测试，没有测试等于没做。

**可测性。** 核心逻辑放在不依赖 QtWidgets 的层里，应用侧的逻辑放在库里而不是可执行文件里。这是为什么模块要这样分。
