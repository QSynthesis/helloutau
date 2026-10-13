# 频谱、频率表与内置的频率表编辑

频率表的英文术语是 frequency map，这是社区的通用说法（作者 2026-10-12 确认），代码中的类型为 `kit::FrequencyMap`，英文注释与界面文字同样使用这个说法。

本文档是音源编辑器第 6 步「频谱与 F0」（[`VoiceBankEditor.md`](VoiceBankEditor.md)）以及路线图第六阶段「频率表」一项（[`Roadmap.md`](Roadmap.md)）的计划，写于 2026-09-30；只读的频率表与频谱已实现，见「进度」。**标为「待定」的各项须由作者决定后才开始实现**，汇总在末尾。作者提出的三个问题分别在「问题一」至「问题三」中讨论。

参考实现是 qfrqeditor，即依据对 masao 的 frqeditor 1.26 的分析重新实现的独立程序，已完成并经作者认可，见 [`claude/frqeditor-reference.md`](claude/frqeditor-reference.md)。其仓库 `E:\Gitee-skyint\frqeditor-reverse` 的 `docs/` 记录了各项行为的依据，下文引用时写作「qfrqeditor 的 `docs/<文件>`」。

## 用语

- **频谱**：对 wav 做短时傅里叶变换得到的幅度，按时间与频率画成图像。
- **频率表**：重采样器为一个 wav 事先分析并保存的基频（F0）序列，每帧一个频率，多数格式另有每帧的振幅。UTAU 的重采样器在缺少频率表时自动生成，并以它为依据变调。
- **格式**：频率表的一种文件形式，与读它的重采样器一一对应。

## 现状

| 方面 | 现状 |
|---|---|
| 格式 | qfrqeditor 读写 6 种：frq（resampler.exe）、vs4ufrq（vs4u.exe）、frc（model4.exe）、pmk（tips.exe）、dio（w4u.exe）、mrq（moresampler.exe），可相互转换 |
| 其余功能 | 编辑曲线、以重采样器生成、检查与修正候选（代替 `checkfrq.dll`）、试听、缩略图、作为 UTAU 插件启动 |
| 本仓库 | 只读的频率表（frq、dio、mrq）与频谱，F0 曲线画在音源窗口的波形上，见「进度」 |

各格式公开资料的情况（qfrqeditor 的 `docs/20260928-public-sources.md`）：frq、dio、mrq 有公开的格式资料；vs4ufrq 的文件由闭源的 `vslib.dll` 读写，只有 Windows 版；frc 与 pmk 没有任何公开资料。

## 问题一：「用哪个重采样器的」

### 频谱与重采样器无关

频谱由 HelloUtau 自己从 wav 计算。frqeditor 也是如此：频谱「数据来自控件自行读取的 wav，与频率表无关」，每 512 个样本一帧、窗长 2048 的 Hamming 窗、2048 点 FFT（qfrqeditor 的 `docs/20260928-frqctrl-paint-and-view.md`「频谱」）。因此频谱不存在「哪个重采样器」的问题，只有计算参数的选择，见「频谱的计算」。

与重采样器有关的只是**叠在频谱上的 F0 曲线**，它来自某一种格式的频率表。

### 频率表与重采样器绑定

同一个 wav 可以同时有几种频率表（`a_wav.frq`、`a.dio`、目录中 `desc.mrq` 的一个条目……），各属于一个重采样器，内容可能不同。frqeditor 的做法是**由当前使用的重采样器的文件名决定编辑哪一种**：`resampler.exe` 以外，`vs4u*.exe`、`model4*.exe`、`tips*.exe`、`w4u*.exe`、`moresampler*.exe` 依次对应一种格式，都不匹配时为 frq（qfrqeditor 的 `docs/20260928-startup-settings.md`「频率表格式」）。

HelloUtau 的方案：

1. **默认按设置中的重采样器判定格式**，规则同 frqeditor。设置中另有一张「重采样器文件名 → 格式」的对照表，内置上面的六条，用户可以增改（例如改名后的 moresampler），插件注册的格式带着自己的规则加入（见问题三）。
2. **波形区旁有一个格式选择框**，列出该 wav 在磁盘上已有的各种频率表，默认选中第 1 条判定的格式；没有该格式的表时，选择框显示「无」，并提供「生成」。
3. 频谱与曲线的帧对齐按所显示格式的帧间隔（frq 为 256 个样本，vs4ufrq 为 220.5），与 frqeditor 相同。
4. **只按文件名识别格式，不执行任何程序。** 生成频率表时才运行重采样器，且只运行设置中的重采样器，不运行工程的 `Tool2`（CLAUDE.md「安全底线」）。

作者 2026-09-30 决定：采用上面的方案，默认按重采样器判定，可切换。

## 问题二：内置的频率表编辑怎么做

### 放在哪里

**建议并入音源窗口，不另开 frqeditor 那样的独立窗口。** frqeditor 的主界面是「原音列表 + 编辑区」，音源窗口已有条目表与波形区，两者对应：

- 波形区增加视图切换：波形（第 3 步的样子）与频谱。频谱视图的纵轴按半音等距（与卷帘相同），叠加所选格式的 F0 曲线，五个值的线仍然画出并可拖动，横轴与波形视图共用。
- 编辑 F0 曲线在频谱视图中进行。手势按 frqeditor（qfrqeditor 的 `docs/20260928-frqctrl-mouse-editing.md`）：拖动画线（帧间线性插值）、右键选区、双击等，另加本仓库的约定：拖动中只改变视图，松开时写入一个撤销步骤，Esc 放弃。
- 「频率表」菜单：生成当前 wav 的表、生成缺少的表、重新生成全部、删除、检查与修正、转换格式。
- 缩略图（frqeditor 的一览窗口）放到最后，可作为条目表的一种视图。

**待定 2**：并入音源窗口，还是像 frqeditor 一样另开「频率表编辑器」窗口（可以独立打开一个文件夹，也可以从音源窗口进入）。作者 2026-09-30 定：先做频谱与只读的频率表，**F0 曲线直接画在波形上**；编辑界面放在哪里到编辑那一步再定。

### 频率表在编辑层中的位置

频率表不宜进入 `VoiceBankSession` 的树：一个音源有几百个 wav，每个表有数千帧，而树中的每个值都进入撤销历史与变更日志。建议：

- 频率表另作一种文档 `FrequencyMapDocument`：一个 wav 的一种格式的表，内存中的帧数组，有自己的撤销历史，按「修改过的帧区间」记录撤销步骤，不经过 substate。
- 音源窗口持有已打开（被编辑过）的表的文档。窗口的未保存标记与 Ctrl+S 同时覆盖音源与这些表：保存音源时写出修改过的表。撤销与重做按焦点所在的区域分开（条目表与波形区的五个值属于音源，频谱视图中的曲线属于当前表）。
- 磁盘变化：`VoiceBankFileSystemState` 目前只记录 wav 与文本文件。频率表被其他程序（重采样器自动生成、frqeditor）改动时须能察觉，至少在打开或切换到该表时核对大小与时间。

**待定 3**：频率表的撤销是否与音源的撤销分开（上面的建议），还是合并为一条历史。合并的好处是 Ctrl+Z 的行为单一，代价是音源的撤销历史中混入大量帧数据。

**待定 4**：保存是否合并（上面的建议：一个 Ctrl+S 同时写出音源与修改过的表），还是频率表立即写入或单独保存。

### 生成与检查

- 生成：对每个 wav 以设置中的重采样器合成一个 50 毫秒的音符（C4，力度 100，无 flags），输出写到临时文件夹后丢弃，利用重采样器缺少频率表时自动生成的行为。frqeditor 运行的是 `"<wav>" nul 100 100 GN 0 50`（qfrqeditor 的 `docs/20260928-resampler-invocation.md`「生成」），HelloUtau 不照搬：参数一律由 `utau::ResamplerArguments::arguments()` 生成（CLAUDE.md「调用外部程序」），`nul` 也只在 Windows 上存在。多个 wav 并行，数目在设置的「频率表」页单独设置，默认为处理器数，可取消。生成线程数不沿用渲染线程数，因为经典模式的播放不用渲染线程（作者 2026-10-12 决定）。参数以向量交给 `SynthToolProcess`，不拼接命令行。
  - 实现（2026-10-12）：`kit::FrequencyMapGenerator`（HelloKitSynth）在后台线程运行，报告进度，结束时列出没有生成频率表的 wav 及重采样器的输出。重新生成先删除已有的表，删除由格式驱动给出（`FrequencyFormat::changesRemoving()`，与改名同样是数据驱动），由 `FrequencyFormats::removeMaps()` 执行。默认实现删除附属文件。mrq 的条目在 `desc.mrq` 中，按 mrq 库的 `mrq_delete_entry()` 把条目名置 0 删除，数据留在原处。删除频率表时一律连同分析缓存（见下文「分析缓存」）。
  - 音源窗口「工具」菜单：Generate Missing Frequency Maps（全部目录中没有表的 wav）、Regenerate Frequency Maps of Selection（选中条目的 wav）、Regenerate All Frequency Maps...（全部 wav，先确认）、Delete All Frequency Maps...（先确认）。格式为设置中的重采样器对应的格式（`formatForResampler()`），与波形上方「F0」框的选择无关，因为生成只能由该重采样器完成。
- 检查与修正：qfrqeditor 以进程内的实现代替 `checkfrq.dll`，与原 DLL 逐帧比较约 486 万帧结果一致，可直接移入。
- 试听：第 4 步的试合成已以设置中的重采样器合成一个音符，编辑频率表后再试合成即可听到效果，不另做 frqeditor 的试听。

### 代码的来源

qfrqeditor 的 `Core` 按本仓库规范重写后移入 hellokit 的新子库（暂名 `HelloKitFrequency`，命名空间 `hello::kit`）：PImpl、命名、注释的来源规则（只引用公开资料）、`std::optional` 表示不存在、编码与路径的规则。界面部分只取行为，按本仓库的场景接口重写。

## 问题三：可扩展的格式

### 抽象

格式驱动（暂名 `FrequencyFormat`）描述一种格式，内置格式与插件注册的格式经同一个注册表（暂名 `FrequencyFormatRegistry`）加入，不分主次，做法同 `InterchangeRegistry`（[`Interchange.md`](Interchange.md)「注册表」）。驱动提供：

| 成员 | 内容 |
|---|---|
| 标识与名称 | 如 `frq`、`mrq`，界面中显示的名称 |
| 重采样器的文件名规则 | 用于问题一的默认判定，可以没有 |
| 存储位置 | 由 wav 的路径得到表的位置。多数格式一个 wav 一个文件（`a_wav.frq`、`a.dio`），mrq 是**每个目录一个数据库** `desc.mrq` 中的一个条目（qfrqeditor 的 `docs/20260928-mrq-database.md`），因此抽象须能表达「目录中的一个条目」，不能只是一个文件路径 |
| 读 | 返回频率表的值，或带原因的诊断 |
| 写 | 接口为 `FrequencyFormat::changesWriting()`，与删除、改名同样给出 `FileChange` 列表，一个目录中多个 wav 的表一次给出（`MapWrite`：wav 文件名、`FrequencyMap`、wav 的采样率、是否手工修改）。`canWrite()` 默认为假，只读格式不实现写出。已有的表中 `FrequencyMap` 不含的部分原样保留。`FrequencyFormats::writeMaps()` 执行它，可选连同分析缓存一起删除（2026-10-12）。各格式的写法见下文「写出」 |
| 删除 | 接口为 `FrequencyFormat::changesRemoving()`，只给出删除频率表本身的 `FileChange` 列表，默认实现删除 `sidecarNameOf()` 所指的附属文件。mrq 覆盖删除，按 mrq 库的 `mrq_delete_entry()` 把条目名置 0（数据留在原处，不整理文件），作为一次 `desc.mrq` 的改写，大小写不同的同名条目一并删除。`FrequencyFormats::removeMaps()` 执行它与 `changesInvalidating()`，即频率表连同分析缓存一起删除（2026-10-12） |
| 分析缓存 | 格式以 `analysisCaches()` 声明重采样器的分析缓存（`AnalysisCache`：显示名称、命名方式与后缀），默认没有。dio 声明 STAR 与 PLATINUM（替换扩展名，world4utau 源码中的 `writeSTARParam()`、`writePlatinumParam()`），mrq 声明 LLSM（追加后缀，实测见下文）。默认实现据此让缓存随 wav 改名，`changesInvalidating()` 给出删除缓存的改动，`FrequencyFormats::invalidateCaches()` 执行，设置页据此列出各格式的缓存（作者 2026-10-12 决定，数据驱动，不与界面耦合） |
| 改名 | wav 改名时给出须执行的操作，驱动本身不改磁盘（作者 2026-10-11 决定，见 [`Editing.md`](Editing.md)「文件名编码错解的处理」）。一个 wav 一个文件的格式把表改名，mrq 改写 `desc.mrq`（照 mrq 库的 `mrq_write_entry()` 在空间不足时的做法，把旧条目名置 0，在末尾追加同样数据、新名字的条目），分析缓存一起改名。一个目录中多个 wav 的改名一次给出，`desc.mrq` 因此只改写一次。调用方把这些操作纳入保存时的检查、执行与回滚。不支持改名的格式，由调用方报告其表没有随 wav 改名。`desc.mrq` 的条目名即 wav 文件名的 UTF-16（实测见下文）。接口为 `FrequencyFormat::changesRenaming()`，以目录中的 `AudioRename` 列表求出 `FileChange` 列表（改名或改写）。每个 wav 旁边一个附属文件（sidecar file）的格式只需实现 `sidecarNameOf()`，默认实现据此让附属文件随 wav 改名。不实现它的格式由默认实现返回 `std::nullopt` 并说明不随 wav 改名，frq 与 dio 已实现，mrq 覆盖实现（2026-10-12） |
| 生成方式 | 「运行对应的重采样器」（默认）或驱动自己的分析（如 vs4ufrq 经 `vslib.dll`，或将来内置的分析器） |

频率表的值（`FrequencyMap`）是各格式的公共部分：帧间隔（`hop`，样本数，可以不是整数，如 220.5）、每帧的时间（毫秒）、频率与可选的振幅、格式自己的「平均频率」。格式特有而公共部分不含的数据由驱动在写出时从已有的文件中保留，保证读入后原样写出（与本仓库保存 UST、oto.ini 时「写回不改变未编辑的内容」的原则相同）。帧间隔为明确的字段而不由帧时间反推，与 qfrqeditor 的 `FrqData::hop` 相同（作者 2026-10-12 决定）。

### 写出

作者 2026-10-12 决定的写法，各格式按其公开资料：

| 格式 | 帧间隔 | 保留的部分 | 新建时 |
|---|---|---|---|
| frq | 须为整数的样本数 | 16 个保留字节 | 保留字节为 0，没有平均频率时以 `FrequencyMap::pitchAverage()` 计算。每帧须有振幅，没有振幅的表不写出 |
| dio | 须等于 world4utau 的帧周期 `FRAMEPERIOD`（5.80498866213152 毫秒）在该采样率下的样本数，因为 world4utau 合成时按这个周期取帧 | 信号长度与采样率，读入后未改的帧时间按原有的字节写出 | 信号长度与采样率取自 wav |
| mrq | 须为整数的样本数 | 其他条目，条目原有的采样率 | 照 mrq 库的 `mrq_write_entry()`：放得下就地覆盖、大小不变，放不下把旧名字置 0 后在末尾追加。文件头的版本改为 2，时间戳为写出的时刻，标志按 `MapWrite::editedByHand` 写 0（手工修改）或 1。新条目的采样率取自 wav，没有 `desc.mrq` 时新建 |

dio 的采样率不按 qfrqeditor 由帧间隔反推（`hop × 44100 / 256` 取整），因为采样率不是 44100 时帧间隔不是整数，取整后写出的采样率与 world4utau 写出的不同。mrq 的采样率同样不照 qfrqeditor 固定为 44100。

平均频率与振幅的算法照搬 qfrqeditor（作者 2026-10-12 决定），代码注释只陈述算法本身的理由：

- 平均频率（`FrequencyMap::pitchAverage()`）：取频率高于 55 Hz 的帧，换算为整数音分（向零取整）后求平均（同样取整），再换算回频率。没有这样的帧时为 C4。在音高刻度上平均，因为音高的感知是对数的。55 Hz 即 A1，低于歌声的音域，不高于它的帧视为无声。
- 振幅：每帧取 wav 第一个声道在该帧的样本（16 位的刻度）的均方根，用于没有振幅的格式转换为 frq。待实施步骤 6「转换」时实现，在此之前 frq 拒绝写出没有振幅的表。

格式之间的转换即「以一种驱动读，以另一种驱动写」，公共部分不能表达的内容在转换时报告为诊断，不静默丢弃（`Interchange.md`「有损转换是常态」）。

### 插件

作者 2026-09-30 决定：**新增第六种插件「频率表格式插件」**（`kit::FrequencyFormatPlugin`），已写入 [`note.md`](note.md)。未采用的做法是归入音源库插件或格式转换插件：频率表格式与它们的接口完全不同，塞进去会使接口含混。与格式转换插件相同，目前只有接口，应用尚无载入插件的机制。

之后作者决定插件只有一种原生插件（[`Plugins.md`](Plugins.md)），频率表格式随之改为它的一个扩展点，`FrequencyFormatPlugin` 已删除，由登记对象 `FrequencyFormatRegistration` 取代；多个格式匹配重采样器时后登记者优先，见 [`Plugins.md`](Plugins.md) 实施步骤 4。

**格式的位置**（作者 2026-09-30 定，2026-10-12 实施）：每种格式各占一个源文件。UTAU 自带的 frq 留在库中（`BuiltinFrequencyFormats`），由编辑器自己登记，因此没有任何插件时也有 frq，与「都不匹配时为 frq」一致（作者 2026-10-12 决定）。dio 与 mrq 移到 FrequencyEditor 插件（`DioFormat`、`MrqFormat`），由插件登记。各格式共用的读取放在 `FrequencyFormat` 的保护成员中（读文件、报告损坏、按附属文件批量查询），二进制数据的读写用 `kit::BinaryStream`（HelloKitSupport，可选大小端，说明了不用 `QDataStream` 的理由）。

### 闭源与无公开资料的格式

- **vs4ufrq** 须经 Windows 上的 `vslib.dll`（1.52 及以上的 64 位版本，作者已定）。建议做成可选的驱动：找不到 `vslib.dll` 时不注册，其他平台上不可用。
- **frc、pmk** 没有公开资料，而本仓库的注释只引用公开资料（`frqeditor-reference.md`「须另行解决的问题」）。**待定 6**：内置这两种格式时注释的来源怎么写；或者不内置，由插件提供（问题三的抽象正好使之可行）。

## 分析缓存

moresampler 等重采样器把从 wav 分析出的结果保存为缓存，其中含有基频，合成时读缓存而不读频率表。频率表改动之后，缓存须删除，合成才会按新的频率表进行。qfrqeditor 对此有两处设置：保存频率表时自动删除缓存，以及重新生成时「频率表与缓存都重新生成」或「只重新生成缓存」。HelloUtau 照此设计，缓存由格式声明（见上表「分析缓存」）。

**实测**（2026-10-12，moresampler 0.8.4，临时文件夹中的副本，`auto-update-llsm-mrq on`，输入为生成的正弦波 wav）：

| 操作 | 再次合成后 |
|---|---|
| 只把条目名置 0，保留 `.llsm` | `desc.mrq` 不变，条目不会重新生成 |
| 条目名置 0，并删除 `.llsm` | 重新分析，在 `desc.mrq` 末尾追加新条目，并重建 `.llsm` |
| 只删除 `.llsm`，保留条目 | 只重建 `.llsm`，`desc.mrq` 不变，即按已有条目的基频重建缓存 |

这与 moresampler 的 readme（0.3.0）一致：创建 `.llsm` 之前才读 `desc.mrq`，缺少条目时补上。`.llsm` 的名字为 wav 的完整文件名加 `.llsm`（`a.wav.llsm`、`あ.wav.llsm`），条目名为 wav 文件名的 UTF-16（`あ.wav` 存为 U+3042）。本机代码页（GBK）无法表示的文件名（`한.wav`），moresampler 直接失败，与 UTAU 的限制相同。

因此删除频率表时一律连同缓存，否则条目不会再生成。设置的「频率表」页（作者 2026-10-12 决定单独一页）：

- 生成线程数：与渲染线程数同样的组合框，单独设置。
- 重新生成时：「删除频率表及其分析缓存」（默认）或「只删除分析缓存，保留频率表」。后者删除缓存后对每个 wav 都运行重采样器（`FrequencyMapGenerator::everyFile`），由重采样器按频率表重建缓存。没有缓存的格式照常删除频率表。
- 分析缓存：每个缓存一行，列出格式、`a.wav` 的频率表文件（`FrequencyFormat::mapFileNameOf()`，默认为附属文件，mrq 为 `desc.mrq`）、缓存名与缓存文件，格式与频率表文件只写在该格式的第一行，没有缓存的格式占一行，插件登记的格式同样列出。各列的宽度可以拖动调整（作者 2026-10-13 要求）。格式名中的重采样器不带 `.exe`。
- 保存频率表时自动删除缓存：待频率表编辑实现时加入，接口已有（`FrequencyFormats::writeMaps()` 的 `invalidatingCaches`）。

## 频谱的计算

- 沿用 frqeditor 的参数：每 512 个样本一帧、窗长 2048、Hamming 窗（qfrqeditor 的 `docs/20260928-frqctrl-paint-and-view.md`「频谱」）。与 frqeditor 的不同：多声道取各声道的平均，与波形区一致；每帧以其位置为中心，超出文件两端的部分按静音计，不像 frqeditor 那样在两端改取开头或末尾的 2048 个样本；不限于 16 位样本。
- FFT 自写（作者 2026-09-30 交我决定）：基 2 的 Cooley–Tukey 迭代算法，2048 点在一个几秒的音频上只需几十毫秒，不值得为此引入依赖。
- 在主线程计算，按音频缓存；绘制时按视图缓存图像。

## 进度

**只读的频率表与频谱**（2026-09-30 完成）：

- 格式层（`HelloKitVoiceBank`）：
  - `kit::FrequencyMap`：各格式的公共部分，每帧的时间（毫秒）、频率（赫兹，0 为无声）与可选的振幅，以及可选的平均频率。
  - `kit::FrequencyFormat`：格式驱动，目前只有标识、名称、重采样器文件名的通配符、`mapsExist()` 与 `read()`；写、删除与生成到后面的步骤再加。`mapsExist()` 一次查询一个目录中的多个 wav（作者 2026-10-12 决定，取代逐个 wav 的 `exists()`），mrq 因此只读一次 `desc.mrq`。
  - `kit::FrequencyFormats`：一个宿主（编辑器）的格式，为 `registry()`（`kit::FrequencyFormatRegistry`，即 `stdc::DynamicRegistry`）中的每个条目创建一个实例；内置格式与插件格式同一途径登记（`Add<V>` 或 `AddFactory`），条目名称即格式 ID，ID 重复的登记失败，实例的 `id()` 与名称不同时被拒绝；`formatForResampler()` 按重采样器的文件名（整名匹配、不分大小写）取匹配的格式，都不匹配时为 frq。不执行任何程序。原先先注册者优先、以 `kit::FrequencyFormatPlugin` 加入插件格式，插件机制改定后改为后登记者优先，见 [`Plugins.md`](Plugins.md) 实施步骤 4。
  - 内置三种有公开资料的格式，注释引用的都是公开资料：
    - frq（`resampler*.exe`）：`a_wav.frq`，布局按 OpenUtau 0.1.565 的 `Frq.cs`；帧时间由帧间隔与 wav 的采样率算出。
    - dio（`w4u*.exe`、`world4utau*.exe`）：`a.dio`，布局按 world4utau 的源码；每帧自带时间。
    - mrq（`moresampler*.exe`）：目录中 `desc.mrq` 里以 wav 文件名为名的条目，布局按 Sleepwalking/mrq；按名称先精确、再不分大小写查找，被删除的条目（名称清零）自然不匹配。
- 频谱（`HelloKitSynth`）：`kit::Spectrogram`，参数见上。
- 界面（音源窗口的波形区 `OtoWaveformView`）：
  - F0 曲线直接画在波形上，无声帧为断开处；纵轴按半音等距，范围是表中有声帧的最低与最高音各外扩一个八度（限于 C1 至 B7，没有表时为 C2 至 C6），每个 C 画一条虚线并在右侧标出音名。
  - 「视图 → Show Spectrogram」：以频谱代替波形，纵轴与曲线共用，五个值的区域与线照常画出、照常拖动。颜色是样式表可设的属性 `frequencyColor`、`spectrumColor`，频谱按峰值以下 60 分贝的范围由背景色渐变到 `spectrumColor`。
  - 试听按钮一行的「F0」下拉框：「None」与各注册格式，默认按设置中的重采样器选中；当前 wav 没有该格式的表时，项目名后加「(none)」。

## 实施步骤（待定项决定之后）

1. **格式层**（读、删除、改名与写出已完成，写出见「写出」）：`FrequencyMap`、`FrequencyFormat`、`FrequencyFormatRegistry`，内置 frq、dio、mrq 三种有公开资料的格式，从 qfrqeditor 重写并移入其测试（含与原版逐字节相同的转换样本）。
2. **频谱视图**（已完成）：波形区的视图切换、频谱的计算与缓存、F0 曲线与格式选择框，只读。
3. **生成**（2026-10-12 完成，见上文「生成与检查」）：以设置中的重采样器生成、生成缺少的、重新生成全部、删除，并行且可取消。
4. **编辑**：`FrequencyMapDocument`、曲线的手势、撤销、与音源一同保存、磁盘变化的核对。
5. **检查与修正**：移入 checkfrq 的进程内实现与修正窗口。
6. **转换与其余格式**：格式间转换；vs4ufrq（Windows，可选）；frc、pmk 按待定 6 的结论。
7. **插件**：按待定 5 的结论开放格式的注册。
8. **缩略图**（可选）。

## 待作者决定的问题

1. ~~格式的判定~~：作者定为默认按重采样器、可切换，已实现。
2. 编辑界面并入音源窗口（建议），或另开频率表编辑器窗口。曲线已直接画在波形上（作者决定），编辑放在哪里到编辑那一步再定。
3. 频率表的撤销与音源分开（建议），或合并为一条历史。
4. 频率表随音源一同保存（建议），或立即写入、单独保存。
5. ~~格式插件~~：作者定为新增第六种插件，已写入 `note.md`。
6. frc、pmk：内置时注释的来源怎么写，或不内置而留给插件。
7. ~~FFT~~：作者交我决定，自写。
