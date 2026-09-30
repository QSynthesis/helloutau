# 与本仓库协作的约定

## 项目概述

HelloUtau 是跨平台的第三方 UTAU 编辑器。目标是在功能上尽可能与官方 UTAU 0.4.19 保持一致，同时重新设计交互、编码处理和扩展机制。

产品形态、工程文件格式（`.usth`）、`_USTH_` 控制音符、`hello-config.json`、编码兼容策略和插件与扩展点的定义，**以 [`docs/note.md`](docs/note.md) 为唯一权威**。该文档与代码冲突时修改代码，不要为迁就实现而修改文档；确需修改设计时，先与作者确认。

## 关于官方 UTAU 的既有事实

不要凭印象描述官方 UTAU 的行为，以下是经过核对的事实。

**最后一个功能版本是 2013/8/31 发布的 v0.4.18**，v0.4.18e（2013/9/5）仅补充了上下文菜单缺失的英文资源。**v0.4.19（2024/5/23）不含任何功能改动，是一次纯安全更新**，官方原文为「セキュリティー脆弱性に対する更新です」，并注明「この問題はエンジンには影響しません」，即引擎（resampler / wavtool）不受影响。v0.4.19(c)（2024/5/24）只更换了安装包，以避免 Windows 11 上「このWindowsインストーラパッケージには問題があります」的报错，安装内容与 v0.4.19 相同。

v0.4.19 修复的是 JVN#71404925 报告的两个漏洞：

| CVE | 类型 | 触发方式 |
|---|---|---|
| CVE-2024-28886 | 操作系统命令注入（CWE-78） | 打开经过构造的 `.ust` 工程文件即可执行任意操作系统命令 |
| CVE-2024-32944 | 路径穿越（CWE-22） | 安装经过构造的音源安装包（`.uar`、`.zip`）可向任意位置释放文件 |

**这两个漏洞的机制决定了本仓库的安全底线**，见下文「安全底线」一节。功能对齐以 v0.4.18 的行为为准，安全设计以 v0.4.19 的结论为准。

结论来源：官方下载页 <https://utau2008.xrea.jp/>（Shift_JIS）、官方博客 <http://utau2008.blog47.fc2.com/>、<https://jvn.jp/jp/JVN71404925/>。

## 参考仓库

六个参考仓库的定位各不相同，不可混用。

**stdutau**（<https://github.com/diffscope/stdutau>，由 `third-party/Dependencies.cmake` 引入）：UTAU 数据层的实现，是依赖而非参考。`utau::UstFile`、`OtoIni`、`PrefixMap`、`PluginInput`、`PluginResult`、`Synth::calc` 已覆盖 ust / oto.ini / prefix.map 的读写、插件临时文件协议和合成参数计算。**凡属这几项功能，一律使用 stdutau，不要在本仓库中重新实现**；发现其功能不足时修改 stdutau，见「与 stdutau 协作」。

**QSynthesis-Old**（<https://github.com/QSynthesis/QSynthesis-Old>）：同一作者 2021 年停止维护的 Qt 5 前作，副本位于 `.cache/QSynthesis-Old`。**只作为行为参考，不作为代码来源。** 值得参考的是它遇到过的实际问题：`Frontend/Process/` 的渲染调度（`RealtimeRenderer` + `ResampleWork` + `ConcatenateWork` 的线程池模型）、`Backend/Documents/Import/` 的 MIDI / VSQ / frq / presamp 导入、`Backend/VoiceBank/` 的音源目录模型。不应迁移的是它的整套 `Q` 前缀类型、自行实现的 `MiniSystem` 基础设施，以及将编码问题推迟到界面层处理的做法。`Synth::calc` 中标有「Port from QSynthesis begin」的音高曲线代码已迁移至 stdutau，不要再从旧仓库迁移一次。

**qsynthesis-revenge / DiffScope**（<https://github.com/SineStriker/qsynthesis-revenge>）：同一作者在 QSynthesis 之后的重写版本，**是同一问题的后续解答，通常比 QSynthesis 的版本正确**。本地没有克隆，需要时按路径获取单个文件。已采用的一处是 `src/plugins/diffscope/iemgr/`（导入导出管理器），本仓库的格式转换模块参照其设计，见 [`docs/Interchange.md`](docs/Interchange.md)。**发现 QSynthesis 中某段代码明显有误时，先在该仓库中查找同一功能的后续版本，再决定实现方式。**

**qtmediate**（<https://github.com/stdware/qtmediate>）：同一作者的 Qt 扩展库，副本位于 `.cache/qtmediate`，原理说明见 `.cache/qsynthesis-docs` 的「3. 元类型」。**只作为主题系统的设计参考，不作为代码来源**：沿用其样式表自定义类型、按钮状态、可着色 SVG 图标与主题组织的思想，代码按本仓库规范重新编写。见 [`docs/Theme.md`](docs/Theme.md)。

**dini**（<https://github.com/diffscope/dini>）：DiffScope 的实验性内存文档引擎，副本位于 `.cache/dini`。**只作为编辑层的设计参考，不作为代码来源**（仓库没有许可证文件）。与 substate 解决同一类问题，但采用关系模型；可参考的是按事务的合并通知、不进入撤销栈的修改及其隐患、恢复数据的结构兼容。见 [`docs/Editing.md`](docs/Editing.md)「参考：dini」。

**synthrt**（`D:\GitHub\synthrt`）与 **stdcorelib**（`D:\GitHub\stdcorelib`）：本仓库工程规范的来源。目录组织、命名、注释、头文件引用参照 synthrt 的 `docs/Development.md`；基础设施优先使用 stdcorelib。

## 技术栈

Qt 6 + CMake + C++17。构建脚本的组织方式参照 synthrt：`find_package(qmsetup)`、`qm_init_directories()`，常量和 `_common_configure_target` 位于 `conf.cmake`。

**qmsetup 由外部提供**，不纳入仓库，通过 vcpkg 获取。

**stdcorelib、stdcorelib.plugin、stdutau、substate 和 QActionKit 均不取自 vcpkg，也均不作为子模块。** 五者都与本仓库同步开发，使用子模块指针会导致每次改动都必须先推送才能使用。分别构建并安装，配置时传入 `-Dstdcorelib_DIR=<prefix>/lib/cmake/stdcorelib`、`-Dstdcorelib-plugin_DIR=<prefix>/lib/cmake/stdcorelib-plugin`、`-Dstdutau_DIR=<prefix>/lib/cmake/stdutau`、`-Dsubstate_DIR=<prefix>/lib/cmake/substate` 和 `-DQActionKit_DIR=<prefix>/lib/cmake/QActionKit`。`third-party/Dependencies.cmake` 统一执行 `find_package`，由根目录的 `CMakeLists.txt` 通过 `include()` 引入；若使用 `add_subdirectory`，导入目标只在该目录作用域内可见，其他模块无法使用。在 Windows 上，该文件还会将 DLL 复制到运行输出目录，vcpkg 的 applocal 不再负责这些库。

**编辑层的文档模型使用 substate**（`stdware/substate`，包含不依赖 Qt 的 `substate` 与存放 `QVariant` 属性节点的 `qsubstate` 两个库），**仅作为 `HelloKitEditBase` 与 `HelloKitEdit` 的私有依赖**：`ss::` 类型不出现在任何公开头文件中（`HelloKitEditBase` 的扩展接口在 `private/` 下，不属于公开接口），节点以 `NodeId` 引用，变更通知由编辑层转换为自己的信号。所有权、事务与撤销的设计见 substate 仓库的 `docs/Design.md`，编辑层的结构见 [`docs/Editing.md`](docs/Editing.md)。

**Windows 的 ANSI 代码页由 `winacp` 转换**（`QSynthesis/winacp`，同样需自行构建安装，传入 `-Dwinacp_DIR=`，**私有依赖**）。UTAU 按写出文件的机器的代码页读写，因此 Shift_JIS、GBK 等编码的转换必须与 Windows 逐字节一致，才能原样往返。Qt 只有在包含 ICU 时才支持这些编码，而 macOS 版 Qt 不包含 ICU；macOS 自带的 Core Foundation 和 iconv 缺少 Windows 映射到私用区的数千个字符，并且会将部分字符编码为与 Windows 不同的字节。`winacp` 是从 Windows 导出的转换表，在三个平台上结果相同。**不要让这些代码页经由 Qt 或系统的转换。**

**MIDI 的解析使用 `wolf-midi`**（去除 Qt 依赖的 `QMidiFile`，vcpkg 端口），同样传入 `-Dwolf-midi_DIR=`，**私有依赖**，不出现在公开头文件中。不要自行实现 MIDI 解析。

**采样率转换使用 r8brain-free-src 6.5**（MIT 许可），由 DiffScope 仓库的 vcpkg 端口（`scripts/vcpkg/ports/r8brain-free-src`）构建为静态库，传入 `-Dunofficial-r8brain-free-src_DIR=<prefix>/share/unofficial-r8brain-free-src`，是 `HelloUtauAudio` 的**私有依赖**。其头文件会引入 `windows.h`，因此该子库定义 `NOMINMAX`。

**菜单、工具栏与快捷键由 QActionKit 提供**（`stdware/qactionkit` 的 `next` 分支，只使用 Core 与 Widgets 两个模块）。动作写在动作扩展清单中，由 AEC 在构建时编译（`qak_add_action_extension()`），用户对菜单的自定义以改动记录保存。清单格式见 qactionkit 仓库的 `docs/action-extension-spec.md`。编辑器的清单与插件的清单都经 `ActionRegistration` 登记，见 [`docs/Plugins.md`](docs/Plugins.md)「编辑界面扩展：动作与命令」。

**stdcorelib 仅作为私有依赖，不出现在公开头文件中。** 子库使用 `LINKS_PRIVATE stdcorelib::stdcorelib`，导出宏使用 `<QtCore/QtGlobal>` 的 `Q_DECL_EXPORT` / `Q_DECL_IMPORT`，不要使用 `STDC_DECL_EXPORT`。两个模块本就依赖 Qt，使用 Qt 的宏不增加额外依赖，而要求下游为一个宏安装 stdcorelib 是不合理的。

**原生插件由 stdcorelib.plugin 载入**（`stdware/stdcorelib.plugin`，构建为动态库，`D:\GitHub\stdcorelib.plugin`），与 stdcorelib 一样是**私有依赖**，插件也不例外：插件类放在插件的 `Internal` 中，不导出、不安装。本仓库不定义插件的基类或接口类，只提供各扩展点的注册接口，见 [`docs/Plugins.md`](docs/Plugins.md)。该库发现不足时直接修改它，改动单独提交到该仓库，遵守其 `AGENTS.md`。

这些库稳定之后再考虑改为子模块。stdutau 改为子模块时，URL 须写为两级的 `../../diffscope/stdutau.git`：helloutau 位于 `QSynthesis` 组织而 stdutau 位于 `diffscope` 组织，一级的 `../stdutau` 会被解析为 `QSynthesis/stdutau`。

## 与 stdutau 协作

stdutau 在本项目开发过程中会被大幅修改并补充测试，应将其视为本仓库的一部分进行修改，而非作为冻结的第三方库绕开。缺少接口就添加接口，行为有误就修正行为，并在 stdutau 的 `tests/` 中补充相应测试。

但须注意：

- **stdutau 有自己的代码风格**（小写文件名、`utau` 命名空间、公有数据成员不带前缀、使用其自己的 `.clang-format`）。在 stdutau 中编写代码时遵循 stdutau 的规范，不要将本仓库的命名规范推广过去。
- stdutau 中「可能不存在的值」一律使用 `std::optional`，不要再引入特殊值。`NODEF_INT` 是唯一的例外，它标记的是稠密音高曲线上的空采样，而非字段缺失。
- **stdutau 的改动单独提交到 stdutau 仓库。** 本仓库目前不记录其版本，因此修改后必须重新构建并安装，否则本仓库使用的仍是旧版本。
- stdutau 的提交信息规范与本仓库相同，只写一行。
- stdutau 不依赖 Qt，也不得使其依赖 Qt。其接口使用 `std::string` 和 `std::filesystem::path`。
- stdutau 不做任何编码转换，读写的是原始字节。这是正确的设计，不要「顺便修正」。

## 编码

这是本项目最容易出错的部分，规则只有一条：**`std::string` 一律为 UTF-8，唯一的例外是即将写入磁盘或刚从磁盘读出的原始字节，这种值不得离开 I/O 边界层。**

- 编码策略由 `docs/note.md` 定义。`.usth` 恒为 UTF-8；`.ust` 的编码记录在控制音符的载荷中，**UST 的 `Charset` 只能表达「是否为 UTF-8」，无法表达具体编码**；导出编码是可配置的应用设置；音源以 `hello-config.json` 为准，插件以 `plugin.json` 为准。**不要猜测编码，也不要以「检测」代替「记录」。**
- `hello-config.json` **每个文件夹一份**，与其描述的 `oto.ini` 位于同一层，而非整个音源一份。
- 转换发生在文件读写层，进入内存模型后只有 UTF-8。界面层不应知道磁盘上是 Shift_JIS 还是 GBK。
- 目标编码无法表示的字符以转义序列表示，见 `docs/note.md`。转义与还原必须是同一段代码的两个方向，实现为一对函数，并配有一组往返测试。
- 路径使用 `std::filesystem::path`，不要经由 `std::string` 中转。在 Windows 上它使用 `wchar_t`，转换为窄字符串会丢失信息。

## 调用外部程序

**使用 `stdc::Popen`，不要使用 `QProcess`。** 其管道是 `std::iostream`，参数在 Windows 上会转换为 UTF-16 再交给 `CreateProcess`，因此任何文字的参数都能原样传递，而这正是 `QProcess` 在本项目中最容易出问题之处。

- 同时打开两个管道时必须使用 `communicate()`。手动交替读取两个管道时，任一管道写满即会死锁，这并非偶发问题。
- 返回码通过 `returnCode()` 获取，类型为 `std::optional<int>`，进程未退出时为 `std::nullopt`，不要用 `value_or(0)` 掩盖。
- 加载 resampler.dll 等动态库使用 `stdc::SharedLibrary`。

resampler / wavtool 的命令行参数由 `utau::ResamplerArguments::arguments()` 和 `utau::WavtoolArguments::arguments()` 生成，不要手动拼接。

## 安全底线

共两条，均来自官方 v0.4.19 修复的两个漏洞。编写任何相关代码之前，先完整阅读本节。

**一、工程文件中的字符串是不可信输入。**

在官方 UTAU 中打开一份经过构造的 `.ust` 可执行任意操作系统命令（CVE-2024-28886，CWE-78）。**官方和 JVN 均未公布具体字段和路径，不要在文档或注释中编造。** 下面第一条给出的是有依据的推断，而非查证的结论。

- **绝不将参数拼接为一条命令行字符串。** 官方 UTAU 渲染时默认写出并执行批处理文件（v0.4.12 的更新日志中有「wav生成時にバッチを使用しない」这一开关，说明默认使用批处理），而写入批处理的文件名、别名、flags 只要包含 `&` 或换行符即可追加命令。这是 CWE-78 最可能的机制。
- **「经由 shell 执行」与「拼接命令行」是两回事，不可混淆。** 非 exe 插件是必须支持的功能，通过 `stdc::Popen::shell(true)` 执行：它保持参数向量的语义，对每个元素进行 `^` 转义并加引号，再包装为 `cmd /d /v:off /s /c`，比 Python 的 `shell=True` 严格得多。禁止的是自行拼接字符串，而非这个开关。
- **`plugin.txt` 的 `shell=use` 对应 `ShellExecuteEx`，而非命令处理器。** [官方规格](https://w.atwiki.jp/utaou/pages/64.html)原文为「通常はCreateProcessでプラグインが起動されますが、shell=useを指定した場合はShellExecuteExで起動されます。これにより、exeファイル以外を実行することができます(jar、html、htaなど)」。即实际运行的是系统为该扩展名注册的处理程序，`plugin.txt` 中并未指定该程序；`.bat` 恰好由 `cmd.exe` 处理，不要将这一巧合当作定义。**HelloUtau 在 Windows 上照 UTAU 沿用 `ShellExecuteEx`**（作者 2026-09-30 决定，见 [`docs/ClassicPluginHost.md`](docs/ClassicPluginHost.md)），不按扩展名自行分派；处理程序不返回进程句柄时，由用户在插件完成后手动确认。
- **默认不使用从工程文件读取的引擎路径。** `Tool1`、`Tool2` 和音符上的 `$patch` 是写在工程中的路径，直接执行等于让工程决定运行哪个程序。应使用本地配置的引擎，除非用户在明确的提示中选择信任。这是「执行任意程序」，与上一条的「注入任意命令」不同，两者都必须防范。
- **但必须原样保存。** 逐工程配置引擎是 UTAU 的正常用法，不保存等于删除用户的设置，那是以安全为借口破坏数据。**需要防范的是未经询问即执行，而非保存。** 这一规则同样适用于 `$patch` 和 `userData`。
- `.ust` 中的所有路径在使用前解析为绝对路径并进行检查，相对路径不得超出工程目录。

**二、解包音源和插件时，压缩包中每个条目的路径都是不可信输入。**

`.uar` / `.zip` 中的条目名可以是 `../../..`，也可以是绝对路径或带盘符的路径，官方 UTAU 按此写入文件，因此可以向任意位置释放文件（CVE-2024-32944）。

- 解包前规范化条目路径，**拒绝绝对路径、盘符、`..` 路径分量和符号链接条目**：不是过滤这些条目，而是拒绝整个压缩包并告知用户。
- 规范化之后再确认目标路径确实位于目标目录之内，使用 `std::filesystem::weakly_canonical` 比较，不要使用字符串前缀比较。
- 解压须设置大小和条目数上限。

这两条须有专门的测试，输入为构造的恶意 `.ust` 和恶意 `.zip`。**若测试未覆盖畸形输入，本节即形同虚设。**

## 代码编写

目录、命名、格式、注释和头文件引用参照 synthrt 的 [`docs/Development.md`](D:/GitHub/synthrt/docs/Development.md)。要点复述如下，冲突时以该文档为准。

本仓库自己的规范是 [`docs/Development.md`](docs/Development.md)，**它才是权威**，以下仅为摘要。

- 共两个模块，每个模块是**一组库**：`hellokit`（命名空间 `hello::kit`，Qt Core，产出 `HelloKitDocument` 等）和 `helloutau`（命名空间 `hello::daw`，Qt Widgets，产出 `HelloUtauEditor` 等以及 `HelloUtau` 可执行文件）。**`hellokit` 不链接 QtWidgets**，核心逻辑不依赖 GUI 才便于测试。
- **应用同样由库和薄驱动组成**，参照 lldb 的 `liblldb` + `tools/driver`。`tools/driver/main.cpp` 只包含入口，其余逻辑位于库中，因为可执行文件无法链接进测试程序，而库可以。
- 每个模块包含一个 `include/` 和一个 `lib/`，参照 synthrt：`hellokit/include/hellokit/Document/` 对应 `hellokit/lib/Document/`。**include 的命名空间是模块名而非目标名**，写 `<hellokit/Document/PayloadCodec.h>`。不使用 `sync_include`。私有头文件与源文件放在一起，加 `_p.h` 后缀，尽量少用。
- **大小写分三个层次**：CMake 包名与 `project()` 小写（`hellokit`、`helloutauConfig.cmake.in`），子库目标与 dll 大驼峰（`HelloKitDocument`），include 命名空间小写。
- 文件名采用大驼峰，与其中的主要类型同名。入口 `main.cpp` 小写；每个子库有一个 `<目标名>Global.h` 存放导出宏。
- 类型采用大驼峰，函数 / 参数 / 变量 / 命名空间采用小驼峰，枚举成员采用大驼峰。私有数据成员使用 `m_` 前缀，PImpl 的两个指针例外，使用 `_impl` 和 `_decl`。**PImpl 采用 stdcorelib 的写法**：`using Decl`，成员函数中用 `stdc_impl_t` / `stdc_decl_t` 取得引用，不直接写 `_impl->` / `_decl->`（见 Development.md「PImpl」）。getter 使用属性名，setter 使用 `set` 加属性名。
- **Qt 的头文件须带模块名**，写 `<QtCore/QByteArray>`、`<QtWidgets/QMainWindow>`，不写 `<QByteArray>`。
- 头文件中引用项目公开头文件时使用尖括号和完整路径；源文件中引用同一目标的头文件时使用双引号。源文件最上方的第一个引用块是同名公开头文件和 `_p.h`，其后依次为系统库、标准库、第三方库、项目内其他目标，当前目标内的其余头文件在最底部单独成块。
- 初始化表达式为指针时写 `auto name = ...`，不写 `auto *name = ...`。析构函数不写 `override`，头文件中被继承的类不写 `final`。
- 命名空间结束处不添加注释。
- 可能不存在结果的函数返回 `std::optional<T>`，不要使用「bool 加输出参数」，也不要用某个特定值表示「不存在」。
- **常量使用小驼峰**，与变量相同，不要写成 `DefaultLyric` 这种形式。大驼峰仅用于类型和枚举成员。
- **契约性常量定义在 `hellokit/Document/DocumentConstants.h`**，不要在使用处就地定义。默认歌词、音域、每拍 tick 数等值，编辑器新建的音符与导入器产出的音符必须一致，定义在两处必然导致二者在不知不觉中产生差异。**UTAU 规定的默认值不在此处重复定义**，stdutau 的 `utaconst.h` 已有定义，应从那里获取。
- 前缀：仓库级 CMake 变量使用 `HELLO_`，模块级 CMake 变量与函数使用 `HELLOKIT_` / `hellokit_`、`HELLOUTAU_` / `helloutau_`，子库导出宏形如 `HELLOKIT_DOCUMENT_EXPORT`，头文件保护按 include 路径命名（`HELLOKIT_DOCUMENT_PAYLOADCODEC_H`）。**模块级前缀必须显式指定**，因为 `qm_setup_build_repo_helpers()` 默认取 `PROJECT_NAME`，而子目录中的 `PROJECT_NAME` 已是 `HelloKitDocument`。
- **带 `Q_OBJECT` 的头文件必须加入目标的 `SOURCES`**，因为 AUTOMOC 只处理 `SOURCES`。位于 `include/` 下的头文件不会被源文件的 glob 匹配，遗漏时链接会缺少四个 moc 符号。

文体（注释、文档、README、帮助文本、诊断消息通用）：

- **采用正式的技术写作文体。** 注释与文档是规范性文本而非叙述。每句陈述一项事实、约束或理由，不写铺垫、感想和修辞。
- **不拟人。** 代码、文件、格式、程序和测试不作为有意志的主语：不写 says、tells、knows、asks、wants、means、cares、decides、promises、is told，也不写「它说」「它知道」「它不认」「它想要」。改用 returns、indicates、records、specifies、reports、detects、requires、rejects，或「返回」「表示」「记录」「规定」「报告」「拒绝」。用户、作者、调用方等真实行为主体可以作主语。
- **用术语，不用描述性转述。** 写 invalid byte sequence，不写 bytes that do not decode。写 Basic Multilingual Plane、unpaired surrogate、reverse mapping、unrepresentable character，不写 the basic plane、half of one、the way back、what cannot be spelled。没有通用术语时，首次出现给出定义，之后始终沿用同一名称。
- **标题、分组名和列表标签用名词或名词短语。** 写 Motivation、Behavior、Supported code pages、Rationale，不写 Why、What it does、What it holds、How it works。能用名词表达时，不用 what 引导的名词从句作主语或宾语：写 the requested encoding，不写 what the user asked for。
- **条件用 if，where 只表示处所。** 不写 empty where there is none，写 empty if absent。不写 nothing where the file is missing，写 \c std::nullopt if the file is missing。不用 one 回指前文名词（such a one、the one it wants），直接重复该名词。
- **不用口语短语。** 不写 whatever else、for good、as it stands、on its own、on the way out、at a glance、there and back、is given up on、the rest of why、and all 这类说法，改为准确的书面表达。
- **句子完整。** 不写片段句、逗号粘连句（两个独立分句只用逗号连接）和反问句。不以 So、And so、Which is why、That is why、Hence 开头叙述因果，改为在同一句中用 because、therefore 表明。不对读者使用第二人称。
- **函数说明以动词开头**（Returns、Decodes、Reads、Rejects）。`\return` 写明每种情况的返回值。布尔查询写 Returns whether …。
- 中文文本同样适用：用书面语，不用「别」「搞」「就行」「得（表必须）」「啥」「拿来」「反正」「其实」「说白了」「这玩意儿」等口语词。标题不用「为什么」「怎么做」，改用「动机」「设计理由」「实现方式」。「不要」「必须」等规范性祈使句不属于口语，照常使用。

注释：

- LLVM 风格，`///` 写在声明上方。**从不使用 `\brief`。**
- 类的 private 成员和 `.cpp` 中的实现细节使用普通的 `//`。私有头文件中的类型和非 private 声明仍使用 `///`。
- Doxygen 命令使用 `\c` `\a` `\note` `\warning` `<tt>`，不要使用反引号或引号。注释中书写以 `@` 开头的字面词时，必须转义为 `\@`。
- **注释中不要使用破折号，也不要用分号连接从句。** 应断句处即断句。使用美式拼写。
- **几个词能说清的内容不写成一段。** 注释说明约束、所有权、生命周期，以及「为何只能如此实现」。不复述签名已经表明的内容，不解释一目了然的重载为何存在，也不将一句话拆成三句。
- **不写考古式注释。** 保留「为何现在必须如此实现」，删除「以前如何、后来修正」。
- 头文件说明调用方据以行动的内容，cpp 说明实现理由，或不写注释。
- 全项目通用的约定写在本文档中，不要在每个使用处重复解释。

Markdown：

- **一段即一行**，不要按 80 或 100 列手动折行。代码块、表格、列表项照常处理。此规则仅适用于 `.md`，C++ 注释仍为 100 列。
- 文件末尾不留多余的空行。

修改后运行 `clang-format`，只对自己改动过的文件运行。**不要用 `sed -i` 处理整个目录。**

## 构建与验证

- **测试使用 QtTest，不使用 Boost.Test。** 本仓库处处依赖 Qt，`QCOMPARE` 能直接打印 `QString` 和 `QByteArray`，而 Boost 需要先逐个转换为 `std::string`；将来 widgets 的测试还需要 `QSignalSpy` 和 `QTEST_MAIN`。stdcorelib 和 stdutau 使用 Boost 是因为它们不依赖 Qt，该惯例不适用于本仓库。
- **`tests/auto` 按模块分目录，与 `include/hellokit` 结构相同**：`tests/auto/Document/` 对应 `include/hellokit/Document/`。每个目录有自己的 `CMakeLists.txt`。
- **程序的测试位于 `tests/auto/tools/`，与 `tools/` 结构相同**：`hellokit/tests/auto/tools/fswatcher/` 对应 `hellokit/tools/fswatcher/`，每个程序一个目录。helloutau 将来有自己的测试时也按此划分。
- **以独立进程运行、通过管道通信的程序，其协议测试可以使用 Python**，由 ctest 调用（启用测试时需要 `Python3`，在 Windows 上使用 `Python3_EXECUTABLE` 避开应用商店的占位 `python`）。测试对象是管道中传递的文本，用脚本编写比用 QtTest 启动进程并手动解析输出更直接。这是 QtTest 规则的例外而非替代：库及其调用层仍使用 QtTest。
- **一个 `test_XXX.cpp` 对应一个 `XXX.h`**，名称一一对应。这样只看目录列表即可知道哪些头文件尚无测试。一个测试文件覆盖三个头文件（如原来的 `test_Interchange.cpp`）时，就无法从目录列表看出这一点。
- **不要重复链接传递依赖。** `HelloKitDocument` 公开链接了 `HelloKitSupport`，测试只需链接 `HelloKitDocument`。
- **确认测试通过之前，先确认构建的退出码为 0。** 构建失败时 ctest 运行的是上一次构建的旧程序，会给出虚假的通过结果。
- **批量修改后核对「修改了几处」，而不只是「能否编译」。** `grep -o <pattern> | wc -l` 统计的是实际出现次数，`grep -c` 统计的是行数。
- 新增测试后确认断言确实被执行，空的测试集同样会「通过」。
- 区分断言的是当前行为还是设计意图。测试可能只是将缺陷固化了下来。
- **完成一项行为的测试后，将缺陷放回，确认测试会失败。** 修改一行、重新构建、运行、还原。
- 能不在测试中引入 Qt 就不引入。核心逻辑位于不依赖 QtWidgets 的层中，才便于测试。

## 提交

- **提交信息不带任何 AI 署名**，不写 `Co-Authored-By`，不写 `Generated with`。
- **只写一行**，使用英文、首字母大写的祈使句和美式拼写，不写正文。设计理由写在代码注释和 `docs/` 中，不要在提交信息中长篇叙述。
- **一个提交只做一件事。** 同一个文件包含两批改动时，用 `git show HEAD:<path>` 取出旧内容，只叠加其中一批改动后提交，再恢复完整版本，不要为图省事暂存整个文件。
- 每个提交自身必须能够构建并通过测试，拆分出的中间状态同样如此。
- **未经授权不要 commit，更不要 push。** 修改完成后保留工作区，待作者同意后再提交。

## 判断与沟通

- **断言之前先验证。** 「公开 API 是这样」不等于「它确实能这样使用」。
- **先测量，再下结论。** 结论必须来自编译器、运行时探针或参考实现，不要凭记忆断言。
- **探针本身也可能出错。** 结果异常时先怀疑探针。
- 用户的质疑通常是正确的，**应先重新验证，而不是辩护**。
- 发现自己有误时直接更正，将结论写回问题清单的对应条目，并注明原判断的错误所在。
- 不确定时明确说明不确定，不要以推测填补。
- 关于官方 UTAU 行为的断言，必须有实测依据或注明出处，**不要转述社区传言**。

## 文档位置

| 内容 | 位置 |
|---|---|
| 产品形态与文件格式定义 | `docs/note.md`，`.usth` 的规格见 `docs/UsthFormat.md` |
| 阶段划分与各阶段的验收标准 | `docs/Roadmap.md` |
| 单个模块的职责边界与接口结构 | `docs/<模块名>.md`，如 `docs/Interchange.md` |
| 值得长期保留的经验与设计记录 | `docs/claude/`（codex 写入 `docs/codex/`） |
| 问题清单、交接、临时分析、参考资料副本 | `.cache/claude/`、`.cache/codex/`（已 gitignore） |
| 面向用户的说明 | `README.md`、`docs/` |
| 项目状态与待办事项 | `docs/Status.md` |

`.cache/` 整体被 gitignore，参考仓库的克隆和下载的官方资料都放在那里，不纳入版本库。

## 已知问题

- **`sed -i` 会将 CRLF 转换为 LF**，而仓库中的行尾是混合的。修改前用 `grep -qU $'\r'` 判断，CRLF 文件改用编辑工具修改。
- **增量构建时，往已有目标中新增或移入带 `Q_OBJECT` 的头文件后，AUTOMOC 可能不为它生成代码**：CMake 已重新配置、`AutogenInfo.json` 已列出该头文件，但 autogen 的缓存认为无须重新扫描，链接时缺少 `staticMetaObject` 等符号。删除该目标的 `<目标名>_autogen` 目录与 `CMakeFiles/<目标名>_autogen.dir/ParseCache.txt` 后重新构建即可，从空目录构建不受影响。
- **`sed` 的替换表达式必须带行号地址**，否则会进行全局替换，且后续表达式会匹配前面已修改的文本，导致层层嵌套。
- **绝不使用 bash heredoc 编写脚本**，也不要经由 shell 传递含反斜杠的 C++ 文本或含日文的字符串，因为 shell 会去掉一层反斜杠。应先写入文件再执行。
- **官方 UTAU 的站点使用 Shift_JIS**，用 `curl` 下载后须显式按 `cp932` 解码，不要让工具猜测。
- **Qt 6 没有 `QTextCodec`**（已移至 Qt5Compat），替代品是 `QStringConverter` / `QStringEncoder` / `QStringDecoder`。**但 `QStringConverter::encodingForName()` 只识别内置的 `Encoding` 枚举**，即 UTF 系列、Latin-1 和 System；`Shift_JIS`、`GBK`、`Big5`、`EUC-KR` 等编码来自 ICU，只能将名称直接传给 `QStringDecoder(name)` / `QStringEncoder(name)` 构造。先将名称转换为枚举会使这些编码全部显示为「不支持」，且不报错。本仓库统一使用 `hello::kit::TextCodec`。
- **`QStringConverter::System` 的 `name()` 返回 `"Locale"`**，这是占位字符串，无法记录到控制音符中。获取具体名称须自行按 `GetACP()` 映射，`TextCodec::systemName()` 即实现此功能。
- **UST 只可能使用两种编码**：写有 `Charset=UTF-8` 的 UTF-8，以及未写任何声明的「写出文件的机器的 ANSI 代码页」。因此需要用户回答的从来不是「两百种编码中的哪一种」，而是「文件在哪个地区写出」，实际上即日本（Shift_JIS）、中国大陆（GBK）、台湾（Big5）。候选列表见 `TextCodec::ustCandidates()`，**不要将 `availableNames()` 的两百多项直接展示给用户**。
- **判断一个字符能否被目标编码表示，不能只看 `hasError()`。** 无法表示时 Qt 写入一个问号且不报错，必须编码后再解码并比较。`TextCodec::canEncode()` 即采用这种方法。
- **`中` 是常用的日文汉字，Shift_JIS 中包含该字。** 需要 Shift_JIS 无法表示的字时，须使用简体专用字，例如 `你`、`简`、`们`。编写编码相关的测试时不要以 `中` 作为反例。
- 在 Windows 上包含 `<windows.h>`：链接了 Qt 的目标使用 `<QtCore/qt_windows.h>`，未链接 Qt 的目标使用 `stdcorelib/platform/windows/stdc_windows.h`。二者都会先定义 `NOMINMAX`。**不要为了一个 `GetACP()` 而额外链接一个库。**
- **CMake 中判断平台而非编译器。** clang 以 `x86_64-pc-windows-msvc` 为目标时，CMake 的 `MSVC` 和 `MINGW` 均为假，`else()` 分支中添加的 `-fPIC` 会导致编译失败。synthrt 根目录的 `CMakeLists.txt` 中有这种写法，参照时须改为 `elseif(NOT WIN32)`。
- **被信号终止的进程不会刷新缓冲的 stdout。** 没有任何输出、看似未运行时，实际上可能是进程已终止。
- **在 Windows 上执行 `.bat` 是有 CVE 记录的注入途径。** `CreateProcess` 遇到 `.bat` 时会交给 `cmd.exe` 再次解析，而 cmd 的规则与 `CommandLineToArgvW` 不同，仅按标准 argv 规则加引号并不足够，这就是 2024 年的 BatBadBut（CVE-2024-24576）。`stdc::Popen::shell(true)` 的 `^` 转义正是针对这一问题，但不能因此将来自工程文件的字符串写入 `.bat` 的参数。
- **`stdc::Popen::shell(true)` 在 Windows 上默认使用 `SW_HIDE`。** 需要 UTAU 那样可见的 cmd 窗口时，在 `startupInfo` 的 `dwFlags` 中加入 `STARTF_USESHOWWINDOW`，此后窗口状态由调用方的 `wShowWindow` 决定。窗口中显示的是子进程写入其控制台的内容，因此要看到输出，就不能将对应的流设为 `Pipe`。本进程已有控制台时，子进程共用该控制台而不新建，`wShowWindow` 对未被创建的窗口不起作用，需要独立窗口时须配合 `creationFlags(CREATE_NEW_CONSOLE)`。**已经实测**：子进程中 `IsWindowVisible(GetConsoleWindow())` 默认为 0，加上该标志后为 1。
- UTAU 音源目录中，同一份 `oto.ini` 可能出现在多级子目录中，`QVoiceBank` 使用 `QMap<QString, QOtoIni>` 是合理的，不要假设一个音源只有一份 `oto.ini`。
- **UTAU 将无法识别的段落当作音符**，而非忽略。读取 UST 遇到未知段名时**静默跳过**（交由 stdutau 处理），写出 UST 时绝不能创建自定义段落。完整的保留规则见 [`docs/claude/utau-ust-preservation.md`](docs/claude/utau-ust-preservation.md)。
