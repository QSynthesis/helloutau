# 项目状态

## 现有内容

仓库骨架按 synthrt 的结构搭建，两个模块均可构建。

| 目标 | 状态 |
|---|---|
| `HelloKitSupport` | `Diagnostic`、`TextCodec`（编码名解析、转义与还原）、`FileSystemWatcher`（磁盘变化提示，由 `hello-fswatcher` 进程实现） |
| `HelloKitDocument` | `PayloadCodec`、`Project` / `Track` / `Note` 模型、`.usth` 读写、`UstDocument` |
| `HelloKitVoiceBank` | `VoiceBankConfig`、`VoiceBankSource`（原始扫描）、`VoiceBank`（解码后的内容与查询，纯值）、`VoiceBankFileSystemState`（磁盘状态：写回、与磁盘核对、重新读取）、`VoiceBankCheckScheduler`、`WaveMetadata`（wav 中 `fmt ` 与 `data` 以外的块的查找与去除）、频率表的只读格式层 `FrequencyFormat` / `FrequencyFormatRegistry` / `FrequencyFormatRegistration`（内置 frq、dio、mrq 由 `BuiltinFrequencyFormats` 登记，FrequencyEditor 插件持有） |
| `HelloKitSynth` | `EngineProcess`、`SynthPlan`（仅计算，含轨道文件在轨道中的起始时刻）、`WaveAudio`（读取 wav）、`Spectrogram`（显示用的频谱）、`WavtoolMixer`（进程内拼接）、`RealtimeSynth`（实时试听的调度与混合）、`PitchCurve`（音符的滑音与颤音曲线，与交给重采样器的曲线逐值相同）、`SampleTiming`（修正后的先行发声、重叠与 STP，与合成相同）、`SynthRunner` 及其实现 `ClassicSynthRunner`、`ThreadedSynthRunner` |
| `HelloKitInterchange` | 接口、注册表 `InterchangeRegistry` 与登记对象 `InterchangeRegistration`、`Formats/MidiConvert`（导入与导出，由 `BuiltinInterchangeDrivers` 登记，Interchange 插件持有） |
| `HelloKitEditBase` | 编辑层的通用部分，命名空间 `hello::kit::edit`：`EditSession`（事务、撤销、变更通知、提交时校验）、`NodeRef`、`Change`、槽位、命令语法，以及扩展接口（字段表、按路径的命令、变更日志） |
| `HelloKitEdit` | 编辑层的文档部分。工程：`ProjectDocument`（打开、导入、保存、是否已修改）、`TrackTimeline`（音符的位置）、`ProjectSession`、句柄 `ProjectRefs`、领域函数 `ProjectEdits`、命令 `ProjectCommands`。音源：`VoiceBankSession`（含保存与从磁盘重新读取）、句柄 `VoiceBankRefs`、领域函数 `VoiceBankEdits`、命令 `VoiceBankCommands` |
| `HelloUtauWidgets` | 通用的控件基础设施：命令面板 `CommandPalette` 与其模糊匹配 `CommandMatcher`；场景接口 `SceneView`、`SceneLayer`、`SceneGesture`，以及随其坐标轴的 `TimelineRuler` 与 `PianoKeyboard` |
| `HelloUtauAudio` | 设备输出 `AudioOutput`（`QAudioSink` 回调接口）、`AudioSource` / `BufferSource` / `StreamSource`（流式，环形缓冲）、采样率转换 `resampled()`（r8brain-free-src）|
| `HelloUtauEditor` | 窗口骨架：QActionKit 清单生成的菜单、工程的打开（UST 编码选择）、保存、另存为与导出 UST、撤销与重做、未保存标记、设置、命令面板（`Ctrl+Shift+P`）；工程在卷帘 `PianoRoll` 中显示与编辑（选区、拖动移调与重排、改长度、笔工具、插入、删除、拆分、歌词就地编辑、量化）；打开工程后读取其音源（按目录选择编码），卷帘标出找不到样本的音符；空格按设置中的播放方式播放（`Playback`）：预渲染以 `temp.bat` 在控制台中渲染选中的音符后播放，实时方式在后台渲染整轨、从播放头直接播放；状态栏显示进度，卷帘显示播放线；「Open Recent」列出最近打开的文件；「显示音高」绘制每个音符的滑音与颤音曲线，并在其上编辑 Mode2 控制点与颤音；Mode2 可在菜单中关闭，此时卷帘显示 Mode1 的曲线并以画笔工具手绘；卷帘下方的参数区编辑包络、力度、调制与速度；复制与粘贴音符、粘贴参数、恢复默认、缩放音高、包络交叉淡化。见 [`Widgets.md`](Widgets.md) 第 1–6 步与 [`Tuning.md`](Tuning.md) 第 1–7 步。音源窗口 `VoiceBankWindow`：条目表的编辑、波形区 `OtoWaveformView` 上拖动与按键设定五个值、试听与以重采样器试合成（`SamplePreview`）、清除音频元数据，见 [`VoiceBankEditor.md`](VoiceBankEditor.md) 第 1–4 步。插件的基础设施：加载器 `AppLoader`（载入原生插件、`--plugin-path` 与 `--settings`、插件列表与启用设置）、设置文件 `settings.json`（`AppSettings`）与 `plugins.json`（`PluginSettings` 格式），动作的登记 `ActionContribution` / `ActionRegistration` / `BuiltinActions`，见 [`Plugins.md`](Plugins.md) |
| `helloutau` | 加载器程序：`main.cpp` 只构造 `AppLoader` 并运行 |
| Core 插件 | 创建 `Editor`，登记编辑器的动作清单（`BuiltinActions`），打开命令行中的文件；设置的「Plugins」页 |
| ClassicPluginHost 插件 | 原版 UTAU 插件：发现、`plugin.txt` / `plugin.json`、临时文件的写出与结果的合并、进程的启动与取消、「工具 → 插件」菜单、首次运行的确认，见 [`ClassicPluginHost.md`](ClassicPluginHost.md)，待作者以社区插件验收 |
| FrequencyEditor 插件 | 登记内置的频率表格式 frq、dio、mrq |
| Interchange 插件 | 登记 MIDI 驱动；「文件 → 导入…」与「文件 → 导出 → 其他格式…」的向导、插入逻辑 `ImportMerge`、MIDI 的编码页，见 [`ImportExport.md`](ImportExport.md)，待作者试用 |

已验证的构建链：qmsetup 的 `hellokit_add_library` / `helloutau_add_library` / `helloutau_add_application`、原生插件的 `helloutau_add_native_plugin`、Qt 6.11 与 AUTOMOC、QActionKit 的清单编译器 AEC、stdcorelib 与 stdcorelib.plugin、stdutau、wolf-midi、QtTest 与 `add_auto_test`、ctest。

`HelloKitSupport` 包含 `Diagnostic` 和 `TextCodec`。编码相关的全部逻辑集中在 `TextCodec`：编码名的解析、无效字节的拒绝（音源另有替换为 U+FFFD 的解码）、不可表示字符的转义。`HelloKitDocument` 包含 `PayloadCodec`、`Project` / `Track` / `Note` 数据模型和 `.usth` 读写。**`.usth` 的读写属于 `Project`**，因为 `.usth` 不是众多格式之一，而是工程本身的序列化形式；其他格式均经由 Interchange 转换为 `Project`。`.ust` 由 `UstDocument` 表示，即一份**已读入但尚未解码**的 UST：`open()` 只解析一次，编码探测与 `toProject()` 均使用该次解析结果。`HelloKitInterchange` 的接口与注册表已完成（`InterchangeReader` / `InterchangeWriter` / `InterchangeSource` / `InterchangeSelector` / `AutomaticSelector` / `InterchangeRegistry` / `InterchangeRegistration`，内置驱动由 Interchange 插件登记），第一个格式 `Formats/MidiConvert` 支持导入与导出，可在无界面环境下运行和测试。结构与约束见 [`Interchange.md`](Interchange.md)。界面由 Interchange 插件提供：导入与导出向导、轨道选择、插入位置与 MIDI 的编码页，见 [`ImportExport.md`](ImportExport.md)。

`HelloKitVoiceBank` 是音源的目录模型。读取与解码分为两步，理由与 `.ust` 相同：编码选择界面必须先向用户展示原始字节，而读取本身不能依赖编码。`VoiceBankSource` 遍历目录树，收集每一级目录的 `oto.ini` / `prefix.map` / `character.txt` / `readme.txt` / `hello-config.json` 和音频文件名，**不做任何解码，也不写入任何文件**：记录编码意味着向用户的音源目录写入文件，扫描阶段不应做此决定。`VoiceBank` 通过 `VoiceBankCharsetSelector`（无界面环境使用 `FixedCharsetSelector`）逐目录获取编码并解码，`find(noteNum, lyric)` 按「prefix.map → 别名 → 文件名」的顺序返回样本及其时间参数。

以下行为已经实测确认：**编码无法确定的目录只丢弃需要解码的部分**，其样本仍可按文件名演唱，因为文件名无需编码，而没有 `oto.ini` 的音源本来就以这种方式演唱；**文件名本身也是别名**，[官方页面](https://w.atwiki.jp/utaou/pages/106.html)说明 UTAU 将 wav 文件名作为别名读取，音源作者以 `_` 前缀排除不希望被演唱的文件；扫描设有深度和目录数上限，并且不跟随任何符号链接，因为音源是用户选择的文件夹，其结构不可信任。

**音源写回已经实现。** 内容（`VoiceBank`）与磁盘状态（`VoiceBankFileSystemState`）分开，二者按目录路径配对；音源按目录保存，每个目录保留读取时使用的编码，`oto.ini` 声明 `#Charset:UTF-8` 时按 UTF-8 读写，其他声明视为不存在。`VoiceBankFileSystemState::save()` 只写入有变化的文件，以原编码写入，拒绝编码无法表示的字符，拒绝磁盘上已被其他程序修改的文件，并在全部检查通过后才开始写入；未读取过的目录作为新目录创建。**一份含 903 个条目的 GBK 真实音源原样打开并保存后，`oto.ini` 逐字节相同；修改一个 offset 只改变一行。** `oto.ini` 按文件名的顺序写出，原本未按此顺序排列的文件在第一次修改时整体重排一次。编码设置分为两种：转换（更改编码后保存，字节改变而文字不变）和重新解读（`reread()`，字节不变而文字改变）。文件名一律按音源自身的编码解码后再拼接路径，不经过系统代码页；否则，编码与系统代码页不一致的音源会得到错误的路径，含 emoji 的文件名会使打开操作抛出异常。详见 [`Editing.md`](Editing.md) 的「音源是第二种文档」一节。

**音源编辑界面打开期间，磁盘上的任何变化都会被检测到。** 监视由独立进程 `hello-fswatcher` 执行。在 Windows 上，它按 JetBrains 的做法只持有**驱动器根目录**的一个句柄，因此音源中的任何目录（包括音源根目录）都可以删除或重命名；进程崩溃后会重启，并在重启后进行全量核对；Debug 构建中也不会弹出阻塞的对话框。监视结果仅作为提示：`VoiceBankFileSystemState::checkDisk()` 使用目录指纹进行核对（只列目录、不读文件，对修改时间过于接近取指纹时刻的文件比较内容），**只检测、不修改**，检测到的变化在 `reloadFromDisk()` 之前每次都会重复报告。`VoiceBankCheckScheduler` 整合了监视提示、定时全量核对、监视失效后的轮询和手动触发；`reloadAllFromDisk()` 忽略指纹，重新读取全部内容。三个平台均有后端实现：Windows 使用 `ReadDirectoryChangesW`，macOS 使用 FSEvents（逐文件事件，不持有任何句柄），Linux 使用 inotify（每个目录单独注册；新目录先注册监视再报告整棵子树；根目录的每一级上级目录也受监视，以便在上级目录重命名时检测到根目录消失）。`hello-fswatcher` 另有 Python 编写的协议测试 `test_fswatcher`（通过 ctest 运行，需要 `Python3`；在 Windows 上用 `Python3_EXECUTABLE` 避开应用商店的占位 `python`）：两个音源并列，覆盖 15 种操作，已在三个平台上运行。在 Linux（Ubuntu 22.04、GCC 11.4、Qt 6.11.2）与 macOS（macOS 26.6.2、arm64、Apple Clang、Qt 6.10.1）上均已完成完整构建，21 项测试全部通过。GB18030 不是 ANSI 代码页，winacp 不提供，在 Windows 上由代码页函数转换，在其他系统上只有 Qt 带 ICU 时可用。macOS 版 Qt 不带 ICU，因此 GB18030 在 macOS 上不可用，`TextCodec` 将其报告为无效编码。

`HelloKitSynth` 已能输出音频，分为三层。`EngineProcess` 启动引擎，**参数以向量传递，不提供接受完整命令行的重载**，这是 CVE-2024-28886 相关安全底线在代码中的体现。`SynthPlan` 只计算不执行，将 `VoiceBank::find` 与 `utau::Synth::calc` 衔接，为每个音符生成两条已解析的参数向量。`SynthRunner` 执行计划，现有两种实现：`ClassicSynthRunner` 写出并执行 UTAU 式的渲染脚本，`ThreadedSynthRunner` 以多线程执行重采样器调用。在这一层中，「原始字节」即 UTF-8：`EngineProcess` 接收 UTF-8，工程本身已是文本，整个过程不涉及转码。

**`wavtool.exe` 不直接写出 wav 文件。** 它向 `<out>.whd`（44 字节文件头）和 `<out>.dat`（PCM 数据）追加内容，二者拼接后才是 wav 文件；官方 UTAU 批处理文件末尾的 `copy /B` 即执行此拼接。渲染开始前必须清除这两个残留文件，否则第二次渲染的输出会追加在第一次之后。

运行器的测试通过 `SynthRunner::makeEngineProcess()` 这一测试接缝注入替身引擎，覆盖参数交付之后的行为，见 [`test_ThreadedSynthRunner.cpp`](../hellokit/tests/auto/Synth/test_ThreadedSynthRunner.cpp) 的文件头注释。

`hellokit/tests/manual/ustrender/` 使用真实音源和真实引擎进行渲染，`--plan` 只打印参数而不执行任何程序。引擎路径必须显式指定，工程中的 `Tool1` / `Tool2` 一律不使用。

`hellokit/tests/manual/ustconv/` 是手动运行的命令行工具，用于集成上述各部分：`.ust` / `.usth` / `.mid` 三种格式两两互转，参数解析使用 `stdc::cli`。它不纳入 ctest，其作用是验证各部分组合后能否正常工作，这是各自的自动测试无法覆盖的。

`ustconv --check <file.ust>` 对应路线图第一阶段「读入后写回，两侧语义相同」这一标准：读取一份 UST，写回临时文件，再读取该文件，用 stdutau 的两次解析结果逐字段比较；如有不一致，报告具体音符、字段及两侧的值，并以非零退出码结束。

比较的对象是**值**而非字节：数字的书写形式由写出方决定；文本先按各自的规则解码再比较，因为两份文件可能编码不同、转义规则也不同（只有含控制音符的文件使用转义），这些都不构成工程上的差异。控制音符在两侧均被跳过，因为它属于设计的一部分，而非数据丢失。

编辑层由通用部分 `HelloKitEditBase`（命名空间 `hello::kit::edit`，稳定后移入 substate）与工程部分 `HelloKitEdit` 组成，设计见 [`Editing.md`](Editing.md)。编辑期间工程是一棵 substate 节点树，`Project` 是从树物化出的快照。修改树的途径只有三层：句柄（`ProjectRefs.h`，按字段种类的类型化函数）、领域函数（`ProjectEdits.h`：`transpose`、`splitNote`、`insertNotes`、`setTempo` 等）和命令（`ProjectCommands.h`，前两层的文本接口）。每个事务是一个撤销步骤，事务可以嵌套，提交时只拒绝本事务新引入的约束违例。变更以 `changed(ChangePtr)` 一个信号报告，也可写成 JSON Lines 的变更日志。通用层不依赖工程的结构，新节点种类经扩展接口注册变更翻译、日志写法和校验。字段表与槽位表是编译期常量。`hellokit/tests/manual/ustedit/` 在无界面环境下以命令编辑 `.usth` 或 `.ust`，可输出变更日志。

音源是编辑层的第二种文档，以同样的方式建在通用层之上。树只含 oto 条目与根目录的三个文件；没有条目的 wav 由磁盘状态记录的音频文件减去条目引用的文件得到。未能读取的子目录（用户没有为其选择编码）不进入树，由会话另行列出；根目录读不了时会话建立失败。每个目录一个编码，`oto.ini` 声明了 UTF-8 时整个目录即为 UTF-8；不合法的字节读作 U+FFFD，修改过而含 U+FFFD 的文件拒绝写出。约束：条目的文件名非空、同一 wav 的别名不重复、`prefix.map` 的键为 24 到 107。领域函数（`VoiceBankEdits.h`）：`setEntry`、`insertEntries`、`includeAudio`、`removeEntries`、`setPrefix`、`removePrefix`、`setCharacter`、`setReadme`、`convertCharset`。从磁盘重新读取是一个撤销步骤，内容读自文件，不校验约束；撤销后树与磁盘不再一一对应，保存时创建树中有而磁盘上没有的目录，磁盘上有而树中没有的目录此后报告为新目录。根目录被删除，或重新读取时没能读取，音源在磁盘上即不完整（`isIncomplete()`），树是唯一完好的副本，保存时整个写回。另存为把音源写到一个不存在或为空的文件夹，默认同时复制原文件夹中的其他文件，也可只写文本文件，此后会话改为编辑新文件夹。`hellokit/tests/manual/voicedit/` 在无界面环境下以命令编辑音源，保存或另存为。

`PayloadCodec` 实现 `_USTH_` 控制音符的载荷编码，即去除填充的 base64url。将其作为第一块代码，并非因为它最重要，而是因为它是纯逻辑、不依赖 Qt，且规则已经实测确定（见 [`claude/utau-ust-preservation.md`](claude/utau-ust-preservation.md)）。

## 依赖来源

- **stdcorelib、stdutau**：**均不取自 vcpkg，也均不作为子模块**，二者都与本仓库同步开发。分别构建并安装，配置时传入 `-Dstdcorelib_DIR=` 和 `-Dstdutau_DIR=`，指向 `<prefix>/lib/cmake/<名称>`。`third-party/Dependencies.cmake` 统一执行 `find_package`，由根目录的 `CMakeLists.txt` 通过 `include()` 引入。在 Windows 上，该文件还会将动态库复制到运行输出目录（目标 `hello_deploy_<包名>`，属于 ALL），vcpkg 的 applocal 不再负责这两个库。stdutau 可构建为静态库或动态库，为动态库时其 DLL 只在完整构建时复制。
- **stdcorelib 仅作为私有依赖**：子库使用 `LINKS_PRIVATE`，公开头文件中的导出宏使用 `<QtCore/QtGlobal>` 的 `Q_DECL_EXPORT` / `Q_DECL_IMPORT`。
- **stdcorelib.plugin**：原生插件的载入与生命周期（`stdware/stdcorelib.plugin`），构建为动态库，同样是私有依赖。与 stdcorelib 相同，自行构建安装后通过 `-Dstdcorelib-plugin_DIR=` 指定，DLL 由 `third-party/Dependencies.cmake` 复制到运行输出目录。本机安装在 `D:/GitHub/stdcorelib.plugin/build/install`，Debug 与 Release 并存。见 [`Plugins.md`](Plugins.md)。
- **winacp**：Windows 全部 ANSI 代码页的转换表，由 Windows 的 `MultiByteToWideChar` / `WideCharToMultiByte` 生成，在三个平台上逐字节一致。`TextCodec` 的 Shift_JIS、GBK、Big5、EUC-KR 以及 `windows-874`、`windows-1250`–`1258` 均由其转换。需自行构建安装，配置时传入 `-Dwinacp_DIR=`。采用它的原因是 macOS 版 Qt 不包含 ICU，原有实现在 macOS 上无法打开任何 Shift_JIS 文件。
- **wolf-midi**：MIDI 的解析与写出，是去除 Qt 依赖的 `QMidiFile`。来自 `E:/GitHub/ds-editor-lite/vcpkg`，同样通过 `-Dwolf-midi_DIR=` 指定。其 `MidiFile.cpp` 使用 `std::log2` 却未包含 `<cmath>`，GCC 下须以 `-DCMAKE_CXX_FLAGS="-include cmath"` 构建。
- **substate**：`HelloKitEditBase` 与 `HelloKitEdit` 的节点树、事务与撤销历史（`stdware/substate`，含 `substate` 与 `qsubstate` 两个库），**仅作为私有依赖**，`ss::` 类型不出现在公开头文件中。与 stdutau 相同，不取自 vcpkg，也不作为子模块，自行构建安装后通过 `-Dsubstate_DIR=` 指定。默认构建为动态库。
- **QActionKit**：菜单、工具栏与快捷键（`stdware/qactionkit` 的 `next` 分支）。与 stdutau 相同，不取自 vcpkg，也不作为子模块，自行构建安装后通过 `-DQActionKit_DIR=` 指定，本机安装在 `E:/GitHub/qactionkit/build/install`。只使用 Core 与 Widgets 两个模块，构建时可以传入 `-DQACTIONKIT_BUILD_QUICK=OFF` 省去 Quick 模块。`HelloUtauEditor` 与各插件的菜单、快捷键和命令面板均由它生成。
- **r8brain-free-src 6.5**：`HelloUtauAudio` 的采样率转换，私有依赖。由 DiffScope 的 vcpkg 端口构建为静态库，本机取自 `E:/GitHub/diffscope-project/vcpkg`，通过 `-Dunofficial-r8brain-free-src_DIR=` 指定。
- **qmsetup**：来自 `D:/GitHub/synthrt/vcpkg`。
- **Qt 6.11.1**：`D:/Qt/6.11.1/msvc2022_64`。

上述路径均记录在 `.vscode/settings.json` 中，该文件已被 gitignore。

## 后续工作

阶段划分、各阶段的验收标准以及从 QSynthesis 沿用的内容，均见 [`Roadmap.md`](Roadmap.md)。**第一阶段「数据层」的四项验收标准均已达成**：转义往返、MIDI 无界面导入、UST 读写语义一致（`ustconv --check`）、真实音源的目录扫描（一份 GBK 编码的中文音源，903 个 oto 条目，`character.txt` 中的作者名和 `Version:1.0` 等非条目行均未丢失）。

**第二阶段「合成」的两项验收标准也已达成。** 第一项「命令行能将 `.ust` 渲染为 wav」：使用真实引擎和真实音源（GBK 编码的中文音源），输出为合法的 44.1 kHz 单声道 16 位 wav 文件。第二项「与 UTAU 渲染同一工程并比较」：所用装置为 `tests/manual/utauprobe` 和 `tests/manual/utaucompare`，在作者亲自调校的一首歌曲上，引擎参数逐项一致，音高曲线 8673 个值的中位偏差为 0，最大偏差为 15 音分。具体数据与判据见 [`Synth.md`](Synth.md)。

缓存管理也已完成：已渲染的音频片段不会重复渲染；缓存文件名是其内容的摘要，因此修改过的音符会自动得到新文件名并重新渲染。见 [`Synth.md`](Synth.md) 的「缓存」一节。

**第三阶段「编辑器骨架」的第一步 `HelloKitEdit` 已完成，七项验收标准均已达成；界面部分按 [`Widgets.md`](Widgets.md) 分六步进行，六步均已实现，第 1–3 步经作者验收，第 4–6 步待作者验收。** 第三阶段遗留的导入界面（轨道选择与插入位置）已由 Interchange 插件实现，见 [`ImportExport.md`](ImportExport.md)。编辑层自身的验收标准见 [`Editing.md`](Editing.md) 末尾，逐项状态如下：

| 验收标准 | 状态 |
|---|---|
| 1. `Project` → 树 → `Project` 往返，两侧逐字段相等 | 达成，`test_ProjectSession` 以完整字段的工程和随机工程检验 |
| 2. 无界面模式打开真实工程，执行命令，撤销到底再重做到底，保存后 UTAU 能正常打开且内容符合预期 | 达成。`ustedit` 编辑作者调校的歌曲 `cuowei.ust`：第 2 个音符 wo 改为 ni 并加颤音，第 12 个音符升两个半音并在 240 tick 拆分、后半歌词 da，第 7 个音符起速度 120。作者在 UTAU 中打开核对，三处均符合预期；未修改的音符与设置和原工程完全相同 |
| 3. 撤销到底后保存的文件与未执行命令时保存的文件语义相同 | 达成，`test_ProjectCommands` 自动检验；`ustedit` 在真实文件上逐字节相同 |
| 4. 同一串命令执行两次，变更序列逐条相同 | 达成，`test_ProjectCommands` 比较两次的变更日志 |
| 5. 节点 ID 在插入、删除、撤销、重做之后仍指向同一节点 | 达成，`test_NodeRef` |
| 6. 音源：修改一条 oto 条目并保存后编码不变，未修改的条目逐字节不变 | 达成。`test_VoiceBankSession_Disk` 自动检验；`voicedit` 在夏语遥音源（Shift_JIS，五个目录，`mid` 等三个目录各 2518 条）的副本上修改 `mid` 的一个 offset 与 `breath sound` 中一条数字全空条目的 cutoff 并保存：两个 `oto.ini` 各只有该行改变，行尾仍为 CRLF，其余数字全空的条目仍为空，其他文件均未重写。这两个文件原本按文件名排列；未按此顺序排列的 `oto.ini` 在第一次修改时整体重排一次，见 Editing.md「读写的保证」 |
| 7. 每个节点操作和领域函数都有对应的命令，由对照两侧列表的测试保证 | 达成。领域函数的类为 `Q_GADGET`，函数标记为 `Q_INVOKABLE`，由 moc 生成的元对象列出；测试将其与命令表中登记的函数对照（`ProjectCommands::domainFunctions()`、`VoiceBankCommands::domainFunctions()`），新增领域函数而未添加命令时测试失败 |

**第四阶段「调音」的验收比较已完成**（2026-09-29）：Mode2 与 Mode1 各一个工程交给 UTAU 合成，参数全部相同，见 [`Tuning.md`](Tuning.md) 与 [`Synth.md`](Synth.md)「调音结果的比较」。

**第五阶段「插件」进行中**：原生插件的载入、设置与「Plugins」设置页，频率表格式与格式转换驱动的登记，ClassicPluginHost 均已实现，验收（社区常用的原版插件）待作者进行。选区编辑与音源批量操作的扩展点尚未建立，见 [`Plugins.md`](Plugins.md)。

## 扩展点位置

插件只有一种原生插件（见 [`Plugins.md`](Plugins.md)），各扩展点的注册接口按归属划分，而非按类型划分：

| 扩展点 | 归属 | 状态 |
|---|---|---|
| 格式转换 | `HelloKitInterchange`（驱动），Interchange 插件（界面与自定义页） | 已实现 |
| 频率表格式 | `HelloKitVoiceBank` | 已实现 |
| 编辑界面扩展 | `HelloUtauEditor`（`ActionContribution`，动作与命令） | 动作已实现，其余随功能加入 |
| 选区编辑、音源批量操作 | `hellokit` | 未建立：选区编辑待第二个使用者出现时从 ClassicPluginHost 提炼 |

**由此 `hellokit` 完全不需要链接 QtWidgets**，「核心不依赖 GUI」与「插件可扩展界面」两项要求不再冲突。

应用由加载器程序与若干动态库组成，原生插件链接这些库（`hellokit` 的子库与 `HelloUtauEditor` 等），插件本身也可作为库供其他插件链接。因此插件须与宿主以同一编译器、同一 Qt 与 hellokit 版本构建，见 Plugins.md「目录」一节的兼容性说明。

## 待定事项

- stdutau 转为子模块的时机。

## 已知问题

均记录在 [`../AGENTS.md`](../AGENTS.md) 的「已知问题」一节，开始工作前应先阅读。
