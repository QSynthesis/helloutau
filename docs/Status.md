# 项目状态

## 现在有什么

仓库骨架按 synthrt 的形状搭好了，两个模块都能构建。

| 目标 | 状态 |
|---|---|
| `HelloKitSupport` | `Diagnostic`、`TextCodec`（编码解析、转义还原） |
| `HelloKitDocument` | `PayloadCodec`、`Project` / `Track` / `Note` 模型、`.usth` 读写、`UstDocument` |
| `HelloKitVoiceBank` | `VoiceBankConfig`、`VoiceBankSource`（原始扫描）、`VoiceBank`（解码后的模型与查询） |
| `HelloKitSynth` | `EngineProcess`、`SynthPlan`（只算）、`SynthRunner`（跑） |
| `HelloKitInterchange` | 接口、注册表、`Formats/MidiConvert`（读写两个方向） |
| `HelloUtauWidgets` | 一个装着 `QLabel` 的 `MainWindow`，证明 Qt Widgets 和 moc 接上了 |
| `helloutau` | 薄驱动，只有 `main.cpp` |

构建链已验证：qmsetup 的 `hellokit_add_library` / `helloutau_add_library` / `helloutau_add_application`、Qt 6.11 加 AUTOMOC、stdcorelib、stdutau、wolf-midi、QtTest 加 `add_auto_test`、ctest。

`HelloKitSupport` 有 `Diagnostic` 和 `TextCodec`，后者是编码这件事的全部落地处：名字怎么解析、解不开的字节怎么拒、编码放不下的字符怎么转义。`HelloKitDocument` 有 `PayloadCodec`、`Project` / `Track` / `Note` 数据模型，以及 `.usth` 的读写——**读写就挂在 `Project` 上**，因为 `.usth` 不是众多格式里的一种，它就是工程本身的写法，别的格式都走 Interchange 转成 `Project`。`.ust` 在 `UstDocument`——它是一份**已经读进来但还没解码**的 UST，`open()` 解析一次，探编码和 `toProject()` 都吃那一次的结果，不重复解析。`HelloKitInterchange` 的接口与注册表齐了（`InterchangeReader` / `InterchangeWriter` / `InterchangeSource` / `InterchangeSelector` / `AutomaticSelector` / `InterchangeRegistry` / `InterchangePlugin`），第一个格式 `Formats/MidiConvert` 读写都有，headless 可跑可测。形状与约束见 [`Interchange.md`](Interchange.md)。**还没有界面**：选轨和选编码那两页要等第三阶段。

`HelloKitVoiceBank` 是音源目录模型。读和解码分成两步，理由和 `.ust` 一样：选编码的那个界面得先把字节拿给用户看，而读这件事本身不能已经需要编码。`VoiceBankSource` 走一遍目录树，把每一级的 `oto.ini` / `prefix.map` / `character.txt` / `readme.txt` / `hello-config.json` 和音频文件名收上来，**什么都不解码、什么都不写**——记住一个编码意味着往用户的音源目录里写文件，扫描不是做这个决定的地方。`VoiceBank` 拿一个 `VoiceBankCharsetSelector`（headless 用 `FixedCharsetSelector`）逐目录问编码，解码，然后 `find(noteNum, lyric)` 按「prefix.map → 别名 → 文件名」给出样本和切割参数。

几条实测钉下来的行为：**没人能说出编码的目录只丢掉需要解码的部分**，它的样本仍然能按文件名唱——文件名不需要编码，而没有 `oto.ini` 的音源本来就是这么唱的；**文件名本身也是别名**，[官方那页](https://w.atwiki.jp/utaou/pages/106.html)写了 UTAU 会把 wav 名当别名读，音源作者靠加 `_` 前缀来避开；扫描有深度和目录数上限，符号链接一律不跟——音源是用户挑的文件夹，形状不归我们信任。

**音源能写回了。** `VoiceBank` 按目录保存，每个目录留着读它用的编码；`save()` 只写变了的文件、按原编码写、编码装不下的字符拒绝、磁盘上被别人改过的拒绝，先全部检查再动手。**一份 903 条的 GBK 真实音源原样打开原样存，`oto.ini` 逐字节相同；改一个 offset 只有一行变。** 设置编码分两种：转换（`setDirectory()` 改编码再存，字节变文字不变）和重新解读（`reread()`，字节不变文字变），转换前 `VoiceBank::isCharsetReadableByUtau()` 说原版 UTAU 在这台机器上读不读得回来。文件名一律按音源自己的编码解开再拼路径，不经过系统代码页——之前那样做，编码和系统代码页不一致的音源路径全错，emoji 文件名直接让打开抛异常。细节见 [`Editing.md`](Editing.md) 的「音源是第二种文档」。

`HelloKitSynth` 能出声了。分三层：`EngineProcess` 起引擎，**参数向量进，没有接受整条命令行的重载**，这是 CVE-2024-28886 那条底线在代码里的形状；`SynthPlan` 只算不跑，把 `VoiceBank::find` 接到 `utau::Synth::calc` 上，产出每个音符两条解析好的参数向量；`SynthRunner` 按轨顺序跑。这一层里「裸字节」就是 UTF-8——`EngineProcess` 收 UTF-8，工程本来就是文本，中间一次转码都没有。

**`wavtool.exe` 不直接写 wav。** 它往 `<out>.whd`（44 字节头）和 `<out>.dat`（PCM）里追加，最后两者拼起来才是 wav；官方 UTAU 的批处理末尾那句 `copy /B` 就是干这个。开跑前两个残留分片也要清，否则第二次渲染接在第一次后面。

**`SynthRunner` 欠着测试**，原因和要补的东西写在 [`test_SynthRunner.cpp`](../hellokit/tests/auto/Synth/test_SynthRunner.cpp) 的文件头注释里：它直接用 `EngineProcess`，测试没地方塞自己的引擎。要开的那个接缝和多线程调度是同一个需求，一起做。

`hellokit/tests/manual/ustrender/` 拿真音源真引擎渲染，`--plan` 只打印参数不跑任何东西。引擎路径必须显式给，工程里的 `Tool1`/`Tool2` 一律不用。

`hellokit/tests/manual/ustconv/` 是手动跑的命令行工具，把上面这些串起来：`.ust` / `.usth` / `.mid` 三种格式两两互转，参数解析用 `stdc::cli`。它不进 ctest，存在的意义就是验证各块拼起来能用——各自的自动测试做不到这件事。

`ustconv --check <file.ust>` 是 Roadmap 第一阶段「读进来写回去、两边语义相等」那一条的载体：读一份 UST，写回一份临时文件，再读回来，拿 stdutau 的两份解析逐字段比，不一致就报出是哪个音符的哪个字段、两边各是什么，退出码非零。

比的是**值**不是字节：数字怎么拼是写的人的事；文本先按各自该用的规则解码再比，因为两份文件可能编码不同、转义规则也不同（控制音符在的那份才有转义），那都不算工程的差异。控制音符本身两边都跳过，它是设计的一部分，不是丢失。

`PayloadCodec` 实现了 `_USTH_` 控制音符的载荷编码，base64url 去填充。选这个作为第一块代码不是因为它最重要，是因为它是纯逻辑、不依赖 Qt、而且规则已经被实测钉死了（见 [`claude/utau-ust-preservation.md`](claude/utau-ust-preservation.md)）。

## 依赖怎么来的

- **stdcorelib、stdutau**：**都不从 vcpkg 拿，也都不是子模块**，两个都在和本仓库一起改。各自构建安装一份，配置时传 `-Dstdcorelib_DIR=` 和 `-Dstdutau_DIR=`，指向 `<prefix>/lib/cmake/<名字>`。`third-party/Dependencies.cmake` 统一 `find_package`，由根 `CMakeLists.txt` `include()` 进来。Windows 上那里还会把动态库拷进运行输出目录，vcpkg 的 applocal 不再管这两个了。stdutau 现在是静态库，所以没有可拷的 DLL。
- **stdcorelib 只做私有依赖**：子库写 `LINKS_PRIVATE`，公开头文件里的导出宏用 `<QtCore/QtGlobal>` 的 `Q_DECL_EXPORT` / `Q_DECL_IMPORT`。
- **wolf-midi**：MIDI 的解析与写出，`QMidiFile` 去掉 Qt 的版本。来自 `E:/GitHub/ds-editor-lite/vcpkg`，一样传 `-Dwolf-midi_DIR=`。
- **qmsetup**：来自 `D:/GitHub/synthrt/vcpkg`。
- **Qt 6.11.1**：`D:/Qt/6.11.1/msvc2022_64`。

路径都写在 `.vscode/settings.json` 里，那个文件是 gitignore 的。

## 接下来

阶段划分、每阶段怎么算数、从 QSynthesis 拿什么不拿什么，都在 [`Roadmap.md`](Roadmap.md)。**第一阶段「数据层」四条算数都过了**：转义往返、MIDI 无界面导入、UST 读写语义相等（`ustconv --check`）、真实音源跑通目录扫描（一份 GBK 的中文音源，903 条 oto 条目，`character.txt` 的作者名和 `Version:1.0` 那种非条目行都没丢）。

**第二阶段「合成」的两条算数也过了**。前半条「命令行能把 `.ust` 渲染成 wav」——真引擎、真音源（GBK 的中文音源），出来是合法的 44.1kHz 单声道 16 位 wav。后半条「和 UTAU 渲染同一工程做比对」——装置是 `tests/manual/utauprobe` 加 `tests/manual/utaucompare`，在作者自己调的一首歌上，引擎参数逐项一致，音高曲线 8673 个读数中位数 0、最差 15 音分。数字和判据在 [`Synth.md`](Synth.md)。

缓存管理也做完了：一块已经渲好的音频不会再渲一次，缓存名是它内容的摘要，所以改过的音符自然换名字、自然重渲。见 [`Synth.md`](Synth.md) 的「缓存」。

## 插件放在哪

按归属分，不按类型分：

| 插件 | 归属 | 理由 |
|---|---|---|
| `RangeEditPlugin`、`VoiceBankPlugin` | `hellokit` | 只动数据，不出界面 |
| `EditorExtensionPlugin` | `helloutau/plugins/` | 要出界面，归应用这一侧 |

**这样 `hellokit` 就彻底不用链接 QtWidgets 了**，「核心不依赖 GUI」和「插件能扩展界面」不再冲突。

插件是运行时加载的 MODULE 库，只需要接口头文件，不反过来链接应用，所以应用不必拆成「共享库加薄驱动」。这一条成立的前提是**接口必须是纯虚类、没有非内联符号**，写接口的时候要守住。

## 还没定的

- stdutau 何时转成子模块。
- `hellokit` 装不装、给不给插件作者用。现在 `HELLOKIT_DEVEL` 是 ON，头文件和 CMake 包都会装出去。
- `EditorExtensionPlugin` 的接口头装不装出去给第三方。装的话 `HELLOUTAU_DEVEL` 要从 OFF 改成 ON。

## 已知的坑

都记在 [`../AGENTS.md`](../AGENTS.md) 的「已知的坑」一节，动手前读一遍。
