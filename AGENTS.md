# 与本仓库协作的约定

## 这是什么

HelloUTAU 是跨平台的第三方 UTAU 编辑器。目标是在功能上尽可能对齐官方 UTAU 0.4.19，同时重做交互、编码处理和扩展机制。

产品形态、工程文件格式（`.usth`）、`_USTH_` 控制音符、`hello-config.json`、编码兼容策略和五类插件的定义，**以 [`docs/note.md`](docs/note.md) 为唯一权威**。那份文档和代码冲突时改代码，不要改文档来迁就实现；确实要改设计，先跟作者确认。

## 关于官方 UTAU 的既有事实

不要凭印象描述官方 UTAU 的行为，以下是核对过的事实。

**最后一个功能版本是 2013/8/31 的 v0.4.18**，v0.4.18e（2013/9/5）只补了上下文菜单缺失的英文资源。**v0.4.19（2024/5/23）不含任何功能改动，是一次纯安全更新**，官方原文：「セキュリティー脆弱性に対する更新です」，并注明「この問題はエンジンには影響しません」，即引擎（resampler / wavtool）不受影响。v0.4.19(c)（2024/5/24）只是换了安装包，为绕开 Windows 11 上「このWindowsインストーラパッケージには問題があります」的报错，安装内容与 v0.4.19 一致。

v0.4.19 修掉的是 JVN#71404925 报告的两个洞：

| CVE | 类型 | 触发方式 |
|---|---|---|
| CVE-2024-28886 | OS 命令注入（CWE-78） | 打开被构造过的 `.ust` 工程文件即可执行任意 OS 命令 |
| CVE-2024-32944 | 路径穿越（CWE-22） | 安装被构造过的音源安装包（`.uar`、`.zip`）可向任意位置释放文件 |

**这两个洞的形状决定了本仓库的安全底线**，见下面「安全底线」一节。做功能对齐时要对齐的是 v0.4.18 的行为，做安全设计时要对齐的是 v0.4.19 的结论。

结论的来源：官方下载页 <https://utau2008.xrea.jp/>（Shift_JIS）、官方博客 <http://utau2008.blog47.fc2.com/>、<https://jvn.jp/jp/JVN71404925/>。

## 参考仓库

四个参考物的定位不同，别混着抄。

**stdutau**（<https://github.com/diffscope/stdutau>，由 `third-party/Dependencies.cmake` 引入）——UTAU 数据层的实现，不是参考物，是依赖。`utau::UstFile`、`OtoIni`、`PrefixMap`、`PluginFileReader/Writer`、`Synth::calc` 已经覆盖了 ust / oto.ini / prefix.map 的读写、插件 tmp 文件协议和合成参数计算。**凡是这几件事，一律走 stdutau，不要在本仓库里重写一份**，发现它不够用就去改它，见「与 stdutau 协作」。

**QSynthesis-Old**（<https://github.com/QSynthesis/QSynthesis-Old>）——同一作者 2021 年停更的 Qt 5 前作，`.cache/QSynthesis-Old` 下有一份。**只当行为参考，不要当代码来源。** 值得看的是它踩过的实际问题：`Frontend/Process/` 的渲染调度（`RealtimeRenderer` + `ResampleWork` + `ConcatenateWork` 的线程池模型）、`Backend/Documents/Import/` 的 MIDI / VSQ / frq / presamp 导入、`Backend/VoiceBank/` 的音源目录模型。不值得搬的是它的整套 `Q` 前缀类型、`MiniSystem` 那套自造基础设施，以及把编码问题拖到 UI 层才处理的做法。`Synth::calc` 里标着「Port from QSynthesis begin」的那段音高曲线代码已经迁到 stdutau 了，不要再从旧仓库里搬一遍。

**qsynthesis-revenge / DiffScope**（<https://github.com/SineStriker/qsynthesis-revenge>）——同一作者在 QSynthesis 之后的重写，**同一个问题的后一版答案，通常比 QSynthesis 那版对**。本地没有克隆，要看就按路径取单个文件。已经用上的一处是 `src/plugins/diffscope/iemgr/`（导入导出管理器），本仓库的格式转换模块照它设计，见 [`docs/Interchange.md`](docs/Interchange.md)。**遇到 QSynthesis 里某段代码明显不对时，先去这个仓库找同一件事的后一版，再决定怎么写。**

**synthrt**（`D:\GitHub\synthrt`）与 **stdcorelib**（`D:\GitHub\stdcorelib`）——本仓库的工程规范来源。目录组织、命名、注释、头文件引用照 synthrt 的 `docs/Development.md`；基础设施优先用 stdcorelib。

## 技术栈

Qt 6 + CMake + C++17。构建脚本的组织方式照 synthrt：`find_package(qmsetup)`、`qm_init_directories()`、`conf.cmake` 里放常量和 `_common_configure_target`。

**qmsetup 由外部提供**，不进仓库，走 vcpkg。

**stdcorelib 和 stdutau 都不从 vcpkg 拿，也都不是子模块。** 两个都在和本仓库一起改，走子模块指针会让每次改动都得先 push 一轮才能用。各自构建并安装一份，配置时传 `-Dstdcorelib_DIR=<prefix>/lib/cmake/stdcorelib` 和 `-Dstdutau_DIR=<prefix>/lib/cmake/stdutau`。`third-party/Dependencies.cmake` 统一 `find_package`，由根 `CMakeLists.txt` `include()` 进来——用 `add_subdirectory` 的话导入目标只在那个目录作用域里，其他模块看不见。Windows 上那里还会把 DLL 拷进运行输出目录，vcpkg 的 applocal 不再管这两个了。

**MIDI 的解析用 `wolf-midi`**（`QMidiFile` 去掉 Qt 的版本，vcpkg 端口），一样传 `-Dwolf-midi_DIR=`，**私有依赖**，不出现在公开头文件里。不要自己写 MIDI 解析。

**stdcorelib 只做私有依赖，不出现在公开头文件里。** 子库写 `LINKS_PRIVATE stdcorelib::stdcorelib`，导出宏用 `<QtCore/QtGlobal>` 的 `Q_DECL_EXPORT` / `Q_DECL_IMPORT`，不要用 `STDC_DECL_EXPORT`。两个模块本来就都依赖 Qt，拿 Qt 的宏不额外欠一笔，而让下游为了一个宏去装 stdcorelib 是不合理的。

两个都稳定下来之后再考虑换成子模块。stdutau 换的时候 URL 要写成两级的 `../../diffscope/stdutau.git`——helloutau 在 `QSynthesis` 组而 stdutau 在 `diffscope` 组，一级的 `../stdutau` 会解析成 `QSynthesis/stdutau`。

## 与 stdutau 协作

stdutau 在本项目开发过程中会被大幅修改和补测试，把它当成本仓库的一部分来改，不是当成冻结的第三方库来绕。缺接口就加接口，行为不对就改行为，同时在 stdutau 的 `tests/` 里补上覆盖。

但是：

- **stdutau 有自己的代码风格**（小写文件名、`utau` 命名空间、公开数据成员不带前缀、`.clang-format` 是它自己那份）。在 stdutau 里写代码照 stdutau 的规矩，不要把本仓库的命名规范推过去。
- stdutau 里「读不到就是没有」一律是 `std::optional`，不要再引入奇异值。`NODEF_INT` 是仅存的例外，它标的是稠密音高曲线上的空采样，不是字段缺失。
- **stdutau 的改动单独提交在 stdutau 仓库里。** 本仓库现在不记录它的版本，所以改完要重新构建并安装一份，否则这边拿到的还是旧的。
- stdutau 的提交信息规矩和本仓库一样，只写一行。
- stdutau 不依赖 Qt，也不要让它依赖 Qt。它的接口是 `std::string` 和 `std::filesystem::path`。
- stdutau 不做任何编码转换，读写的是原始字节。这是对的，不要「顺手修好」。

## 编码

这是本项目最容易出错的地方，规则只有一条：**`std::string` 一律是 UTF-8，唯一的例外是即将写入磁盘或刚从磁盘读出的原始字节，这种值不许离开 I/O 边界那一层。**

- 编码策略由 `docs/note.md` 定义。`.usth` 恒为 UTF-8；`.ust` 的编码记在控制音符的载荷里，**UST 的 `Charset` 只能表达「是不是 UTF-8」，表达不了是哪种编码**；导出编码是可配置的应用设置；音源看 `hello-config.json`，插件看 `plugin.json`。**不要猜编码，也不要用「检测」代替「记录」。**
- `hello-config.json` **一个文件夹一个**，和它描述的 `oto.ini` 放在同一层，不是整个音源一份。
- 转换发生在文件读写层，进了内存模型就只有 UTF-8。UI 层不该知道磁盘上是 Shift_JIS 还是 GBK。
- 目标编码表示不了的字符用转义串，见 `docs/note.md`。转义和还原必须是同一处代码的两个方向，写成一对函数，配一组往返测试。
- 路径用 `std::filesystem::path`，不要用 `std::string` 转手。Windows 上它是 `wchar_t`，转成窄串就丢信息。

## 调用外部程序

**用 `stdc::Popen`，不要用 `QProcess`。** 管道是 `std::iostream`，参数在 Windows 上会转成 UTF-16 再交给 `CreateProcess`，所以任何脚本写的参数都能原样到达，这正是 `QProcess` 在本项目里最会出问题的地方。

- 开了两个管道就必须用 `communicate()`。手工轮流读两个管道，一个写满就死锁，这不是偶发。
- 返回码是 `returnCode()`，类型是 `std::optional<int>`，没退出就是 `std::nullopt`，不要 `value_or(0)` 糊过去。
- 加载 resampler.dll 这类动态库用 `stdc::SharedLibrary`。

resampler / wavtool 的命令行参数由 `utau::ResamplerArguments::arguments()` 和 `utau::WavtoolArguments::arguments()` 生成，不要手拼。

## 安全底线

两条，都来自官方 v0.4.19 修的那两个洞。写到任何一条相关的代码时，先把这里读完。

**一、工程文件里的字符串是不可信输入。**

打开一个构造过的 `.ust`，在官方 UTAU 上可以执行任意 OS 命令（CVE-2024-28886，CWE-78）。**具体字段和路径官方与 JVN 都没有公布，不要在文档或注释里编一个出来。** 下面第一条给出的是有据可依的推断，不是查到的结论。

- **永远不要把参数拼成一条命令行字符串。** 官方 UTAU 渲染时默认写批处理再执行（v0.4.12 的更新日志里有「wav生成時にバッチを使用しない」这个开关，说明默认是用），而批处理里插进去的文件名、别名、flags 只要含 `&` 或换行就能追加命令。这是 CWE-78 最可能的形状。
- **「走 shell」和「拼命令行」是两回事，不要混。** `.bat` 插件是必须支持的功能（`plugin.txt` 里有 `shell` 键），走 `stdc::Popen::shell(true)`——它保持参数向量的语义，逐个元素做 `^` 转义再加引号，包成 `cmd /d /v:off /s /c`，比 Python 的 `shell=True` 严格得多。禁止的是自己拼串，不是这个开关。
- **从工程文件读到的引擎路径，默认不使用。** `Tool1`、`Tool2` 和音符上的 `$patch` 是工程里写死的路径，照单执行等于让工程决定跑什么程序。用本地配置里的引擎，除非用户在明确的提示里选择信任。这是「执行任意程序」，和上一条的「注入任意命令」是两回事，两条都要防。
- **但要原样存下来。** 逐工程配置引擎是 UTAU 的正常用法，不存等于删用户的设置，那是拿安全当借口破坏数据。**要防的是不问就执行，不是持有。** 这条对 `$patch` 和 `userData` 一样成立。
- `.ust` 里所有路径在使用前解析为绝对路径并检查，相对路径不许逃出工程目录。

**二、解包音源和插件时，压缩包里的每个条目路径都是不可信输入。**

`.uar` / `.zip` 里的条目名可以是 `../../..`，也可以是绝对路径或带盘符，官方 UTAU 照着写文件，于是可以往任意位置释放文件（CVE-2024-32944）。

- 解包前规范化条目路径，**拒绝绝对路径、盘符、`..` 分量和符号链接条目**，不是过滤掉它们，是拒绝整个包并告诉用户。
- 规范化之后再确认目标路径确实位于目标目录之内，用 `std::filesystem::weakly_canonical` 比较，不要用字符串前缀比较。
- 解压要有大小和条目数上限。

这两条要有专门的测试，输入就是构造出来的恶意 `.ust` 和恶意 `.zip`。**测试没有覆盖畸形输入，这一节就等于没写。**

## 代码编写

目录、命名、格式、注释和头文件引用照 synthrt 的 [`docs/Development.md`](D:/GitHub/synthrt/docs/Development.md)。要点复述如下，冲突时以那份文档为准。

本仓库自己的那份是 [`docs/Development.md`](docs/Development.md)，**它才是权威**，下面只是提要。

- 两个模块，每个是**一族库**：`hellokit`（命名空间 `hello::kit`，Qt Core，产出 `HelloKitDocument` 等）和 `helloutau`（命名空间 `hello::daw`，Qt Widgets，产出 `HelloUtauWidgets` 等加 `helloutau` 可执行文件）。**`hellokit` 不链接 QtWidgets**，核心逻辑不依赖 GUI 才测得动。
- **应用也是库加薄驱动**，照 lldb 的 `liblldb` + `tools/driver`。`tools/driver/main.cpp` 只放入口，其余在库里——可执行文件没法链进测试二进制，库可以。
- 模块级一个 `include/` 一个 `lib/`，照 synthrt：`hellokit/include/hellokit/Document/` 配 `hellokit/lib/Document/`。**include 的命名空间是模块名不是目标名**，写 `<hellokit/Document/PayloadCodec.h>`。不用 `sync_include`。私有头同源文件放，加 `_p.h` 后缀，尽量少用。
- **大小写三层**：CMake 包名与 `project()` 小写（`hellokit`、`helloutauConfig.cmake.in`），子库目标与 dll 大驼峰（`HelloKitDocument`），include 命名空间小写。
- 文件名大驼峰，与其中的主要类型同名。入口 `main.cpp` 小写；每个子库一个 `<目标名>Global.h` 放导出宏。
- 类型大驼峰，函数 / 参数 / 变量 / 命名空间小驼峰，枚举成员大驼峰。私有数据成员 `m_` 前缀，PImpl 的两个指针例外，用 `_impl` 和 `_decl`。getter 是属性名，setter 是 `set` 加属性名。
- **Qt 的头文件要带模块名**，写 `<QtCore/QByteArray>`、`<QtWidgets/QMainWindow>`，不写 `<QByteArray>`。
- 头文件里引用项目公开头用尖括号全路径；源文件里同目标的头用双引号。源文件最上方第一个引用块是同名公开头和 `_p.h`，然后依次是系统库、标准库、第三方库、项目内其他目标，当前目标内其余头文件在最底部单独成块。
- 初始化表达式是指针时写 `auto name = ...`，不写 `auto *name = ...`。析构函数不写 `override`，头文件里被继承的类不写 `final`。
- 命名空间结束处不加注释。
- 读不到就是没有的地方返回 `std::optional<T>`，不要用「bool 加出参」，也不要拿某个特定值当「没有」。
- 前缀：仓库级 CMake 变量 `HELLO_`，模块级 CMake 变量与函数 `HELLOKIT_` / `hellokit_`、`HELLOUTAU_` / `helloutau_`，子库导出宏 `HELLOKIT_DOCUMENT_EXPORT` 这类，头文件保护跟 include 路径走（`HELLOKIT_DOCUMENT_PAYLOADCODEC_H`）。**模块级的前缀必须显式给**，`qm_setup_build_repo_helpers()` 默认取 `PROJECT_NAME`，而子目录里那个已经是 `HelloKitDocument` 了。
- **带 `Q_OBJECT` 的头文件必须进目标的 `SOURCES`**，AUTOMOC 只扫 `SOURCES`。头在 `include/` 下不会被源文件 glob 捞到，漏了就链接时缺四个 moc 符号。

注释：

- LLVM 风格，`///` 写在声明上方。**从不用 `\brief`。**
- 类的 private 成员、`.cpp` 里的实现细节用普通 `//`。私有头文件里的类型和非 private 声明仍用 `///`。
- Doxygen 命令用 `\c` `\a` `\note` `\warning` `<tt>`，不要用反引号或引号。注释里要写字面量 `@` 开头的词必须转义成 `\@`。
- **注释里不要破折号，也不要用分号连接从句。** 该断句就断句。美式拼写。
- **不写考古式注释。** 「为什么现在必须这么写」留下，「以前是什么样、后来修了」删掉。
- 头文件写调用方要据此行动的东西，cpp 写为什么这么做，或者干脆不写。设计理由属于提交信息。
- 全项目通用的约定写在这份文档里，不要在每个使用点重复解释。

Markdown：

- **一段就是一行**，不要按 80 或 100 列手动折行。代码块、表格、列表项照旧。这条只管 `.md`，C++ 注释仍是 100 列。
- 文件结尾不留多余换行。

改完跑 `clang-format`，只对自己动过的文件跑。**不要 `sed -i` 扫全目录。**

## 构建与验证

- **看到测试通过之前，先确认 build 的退出码是 0。** 构建失败时 ctest 跑的是上一轮的旧二进制，会给出虚假的绿色。
- **批量改动后核对「改了几处」，而不是「能不能编译」。** `grep -o <pattern> | wc -l` 数的是实际次数，`grep -c` 数的是行数。
- 新增测试后确认断言真的执行了，空 suite 也会「通过」。
- 分清断言的是当前行为还是设计意图。测试可能只是把缺陷固化了下来。
- **写完行为要把缺陷放回去，确认测试会变红。** 改一行、重建、跑、还原。
- Qt 的东西能不进测试就不进测试。核心逻辑放在不依赖 QtWidgets 的层里，才测得动。

## 提交

- **提交信息不带任何 AI 署名**，不写 `Co-Authored-By`，不写 `Generated with`。
- **只写一行**，英文，首字母大写的祈使句，美式拼写，不写正文。「为什么」留给代码注释和 `docs/`，不要写成提交信息里的小作文。
- **一个提交一件事。** 同一个文件承载两批改动时，用 `git show HEAD:<path>` 取出旧内容、只叠加其中一批、提交、再放回完整版本，不要图省事整文件暂存。
- 每个提交自身必须能构建、能通过测试，拆分出来的中间状态也一样。
- **未经授权不要 push。**

## 判断与沟通

- **断言之前先验证。** 「公开 API 长这样」不等于「它真的能这么用」。
- **先量再说。** 结论要么来自编译器、要么来自运行时探针、要么来自参考实现，不要凭记忆断言。
- **探针本身也会骗人。** 结果反常时先怀疑探针。
- 用户的质疑基本都是对的，**先重新验证，不要辩护**。
- 发现自己错了就直接更正，把结论写回问题清单的对应条目，注明原判断错在哪。
- 不确定就说不确定，不要用推测填补。
- 涉及官方 UTAU 行为的断言，要么有实测，要么标明出处，**不要转述社区传言**。

## 文档去处

| 内容 | 位置 |
|---|---|
| 产品形态与文件格式定义 | `docs/note.md`，`.usth` 的规格在 `docs/UsthFormat.md` |
| 阶段划分与每阶段的完成判据 | `docs/Roadmap.md` |
| 单个模块的职责边界与接口形状 | `docs/<模块名>.md`，如 `docs/Interchange.md` |
| 值得长期保留的经验、设计记录 | `docs/claude/`（codex 写 `docs/codex/`） |
| 问题清单、交接、临时分析、参考资料副本 | `.cache/claude/`、`.cache/codex/`（已 gitignore） |
| 面向用户的说明 | `README.md`、`docs/` |
| 项目状态与 TODO | `docs/Status.md` |

`.cache/` 整个是 gitignore 的，参考仓库的克隆、下载下来的官方资料都放那儿，不要进版本库。

## 已知的坑

- **`sed -i` 会把 CRLF 拍平成 LF**，而仓库里行尾是混的。动手前 `grep -qU $'\r'` 判断，CRLF 文件改用编辑工具。
- **`sed` 的替换表达式必须带行号地址**，否则是全局替换，且后续表达式会匹配前面已改过的文本，层层嵌套。
- **绝不用 bash heredoc 写脚本**，也不要经 shell 传含反斜杠的 C++ 文本或含日文的字符串。shell 会吃掉一层反斜杠。用写文件的方式落盘再执行。
- **官方 UTAU 的站点是 Shift_JIS**，`curl` 下来要显式按 `cp932` 解码，别让工具猜。
- Windows 上包含 `<windows.h>` 要用 `stdcorelib/platform/windows/stdc_windows.h`，它会先定义 `NOMINMAX`。
- **CMake 里判平台不要判编译器。** clang 目标 `x86_64-pc-windows-msvc` 时，CMake 的 `MSVC` 和 `MINGW` 都是假，`else()` 兜底加的 `-fPIC` 会直接把它编译不过。synthrt 的根 `CMakeLists.txt` 里有这个写法，抄的时候要改成 `elseif(NOT WIN32)`。
- **被信号杀死的进程不会 flush 缓冲的 stdout。** 输出一个字都没有、看起来像没跑，其实是死了。
- **Windows 上执行 `.bat` 是个有 CVE 记录的注入面。** `CreateProcess` 遇到 `.bat` 会转交 `cmd.exe` 二次解析，而 cmd 的规则和 `CommandLineToArgvW` 不同，光按标准 argv 规则加引号不够——这就是 2024 年的 BatBadBut（CVE-2024-24576）。`stdc::Popen::shell(true)` 的 `^` 转义是冲着它去的，但别因此往 `.bat` 的参数里塞工程文件来的字符串。
- **`stdc::Popen::shell(true)` 在 Windows 上默认 `SW_HIDE`。** 要 UTAU 那种可见的 cmd 窗口，`startupInfo` 的 `dwFlags` 带上 `STARTF_USESHOWWINDOW` 即可，带了就由调用方的 `wShowWindow` 说了算。窗口里显示的是子进程写到自己控制台的东西，所以要看见输出就不能把那条流设成 `Pipe`。本进程已有控制台时子进程是共用它而不是新开一个，`wShowWindow` 对一个压根没被创建的窗口不起作用，要独立窗口得配 `creationFlags(CREATE_NEW_CONSOLE)`。**已实测**：子进程里 `IsWindowVisible(GetConsoleWindow())` 默认为 0，带上那个标志为 1。
- UTAU 音源目录里同一个 `oto.ini` 可能出现在多级子目录，`QVoiceBank` 用 `QMap<QString, QOtoIni>` 是有道理的，不要假设一个音源只有一份 `oto.ini`。
- **UTAU 把它不认识的段落当成音符**，不是忽略。读 UST 遇到未知段名要当文件已损坏处理，写 UST 绝不能自造段落。完整的保留规则见 [`docs/claude/utau-ust-preservation.md`](docs/claude/utau-ust-preservation.md)。
