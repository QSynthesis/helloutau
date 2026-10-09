# 代码编写原则

本文档规定 helloutau 仓库通用的代码组织与 C++ 编写原则。具体格式以仓库根目录的 `.clang-format` 为准。

本文档与 synthrt 的 `docs/Development.md` 保持一致。两者冲突时以本文档为准，但不应为了本仓库的便利而分叉规则，应先确认 synthrt 的规则是否也需要修改。

## 模块

共两个模块，每个模块是**一组库**而非单个库：

| 模块 | 命名空间 | 产出 | 依赖 |
|---|---|---|---|
| `hellokit/` | `hello::kit` | `HelloKitDocument`、今后的 `HelloKitCore` 等 | Qt Core、stdutau、stdcorelib、substate（私有，仅 `HelloKitEditBase` 与 `HelloKitEdit`） |
| `helloutau/` | `hello::daw` | `HelloUtauEditor` 等、随应用提供的原生插件，以及 `helloutau` 可执行文件 | Qt Widgets、hellokit、QActionKit、stdcorelib.plugin |

stdcorelib 与 stdcorelib.plugin 可以出现在公开头文件中（作者 2026-10-08 决定），见 CLAUDE.md。公开头文件用到它们的子库以 `LINKS` 公开链接，其余子库以 `LINKS_PRIVATE` 链接。

`hello` 仅作为外层命名空间，代码一律位于第二层。不要在 `hello` 中直接声明内容，也不要再增加第三层。

**例外：`HelloKitEditBase` 位于第三层命名空间 `hello::kit::edit`。** 它是编辑层与文档无关的通用部分，稳定后将移入 substate，第三层命名空间使 `Slot`、`Range`、`Change`、`NodeRef` 等通用的名字在此之前不占用 `hello::kit`。移走后此例外取消。见 [`Editing.md`](Editing.md) 的「通用层与文档层」。

**`hellokit` 不链接 QtWidgets。** 界面属于应用程序，核心逻辑不依赖 GUI 工具包才便于测试。

**应用同样由库和一个薄驱动组成**，结构参照 lldb 的 `liblldb` 与 `tools/driver`。`tools/driver/main.cpp` 只包含入口，其余逻辑均位于库中，因此应用侧的逻辑同样可以测试：可执行文件无法链接进测试程序，而库可以。入口只构造加载器 `AppLoader` 并运行，创建编辑器、打开窗口由 Core 插件负责，见 [`Plugins.md`](Plugins.md)。

**不涉及界面的逻辑放在 `hellokit`，即使只有应用使用它。** 打开、保存、判断是否已修改的 `ProjectDocument` 属于 `HelloKitEdit`：它只需 Qt Core，无界面工具同样可以使用；需要询问用户之处以回调接口交给界面一侧实现，如 `UstCharsetSelector`、`VoiceBankCharsetSelector`。

`helloutau` 的子库按层次划分，上层依赖下层：

| 子库 | 内容 |
|---|---|
| `HelloUtauEditor` | 应用层：窗口、对话框、菜单与快捷键的清单、设置 |
| `HelloUtauWidgets` | 通用的控件基础设施，与 UTAU 和 hellokit 无关，类似对 qtbase 的补充 |
| `HelloUtauTheme` | 主题系统，见 [`Theme.md`](Theme.md) |
| `HelloUtauAudio` | 音频设备的输出，见 [`Widgets.md`](Widgets.md) |
| `HelloUtauTesting<子库>` | 测试的辅助库，供本仓库与下游项目的测试链接 |

`HelloUtauTheme` 在第一次有内容时建立。

**测试的辅助库参照 LLVM 的 `llvm/lib/Testing`。** `include/helloutau/Testing/` 与 `lib/Testing/` 下按所服务的子库分目录，文件名为 `Testing` 加所服务的头文件名：`<helloutau/Testing/Editor/TestingEditor.h>` 服务于 `<helloutau/Editor/Editor.h>`，构建为 `HelloUtauTestingEditor`。文件名不与所服务的头文件同名，因此辅助库自身的测试 `test_TestingEditor.cpp` 与 `test_Editor.cpp` 不会重名，源文件的引用也不会被解析到所服务的头文件。LLVM 以目录为单位构建测试程序，同名的测试文件因此分属 `ADTTests` 与 `TestingADTTests`。本仓库以文件为单位构建测试程序，因此以文件名区分。只有头文件的目录不建库，与 LLVM 的 `Testing/ADT` 相同。命名空间与所服务的代码相同，不另设一层。这些库为动态库，只在 `HELLO_BUILD_TESTS` 或 `HELLO_INSTALL_TESTING` 打开时构建，只在后者打开时安装，因此面向用户的安装不包含这些库和 Qt Test。头文件随 `HELLO_DEVEL` 安装。

## 目录与文件

每个模块包含一个 `include/` 和一个 `lib/`，各子库在其中各占一个目录，结构参照 synthrt：

```
hellokit/include/hellokit/Document/PayloadCodec.h     ← #include <hellokit/Document/PayloadCodec.h>
hellokit/lib/Document/PayloadCodec.cpp                ← 目标 HelloKitDocument

helloutau/include/helloutau/Editor/ProjectWindow.h
helloutau/lib/Editor/ProjectWindow.cpp           ← 目标 HelloUtauEditor
helloutau/plugins/Core/                          ← 目标 CorePlugin，输出到 lib/plugins/helloutau/Core；
                                                    公开头文件在此，以 <Core/...> 引用
helloutau/plugins/Core/Internal/CorePlugin.cpp   ← 插件类与其余实现，不安装
helloutau/tests/auto/plugins/                    ← 插件中库一级内容的测试，以及只供测试的插件
helloutau/tools/driver/main.cpp                  ← 目标 helloutau
```

**include 的命名空间是模块名，而非目标名。** 写 `<hellokit/Document/PayloadCodec.h>`，不写 `<HelloKitDocument/PayloadCodec.h>`。`HelloKitDocument` 仅是产出的动态库文件名。

不使用 qmsetup 的 `sync_include`，`include/` 是实际存在的目录。

仅供实现使用的私有头文件放在源文件旁边，并使用 `_p.h` 后缀。私有头文件会增加实现之间的耦合，应尽量少用。

**例外：供其他目标实现时使用的私有头文件放在 `include/<模块>/<子库>/private/` 下**，参照 Qt 的做法，目前只有 `HelloKitEditBase` 的扩展接口。其中可以出现私有依赖的类型（如 `ss::`），因此使用它的目标须自行链接该依赖。它们不属于公开接口，文档层以外的代码不应使用。

仅供多个实现文件复用且不独立编译的实现片段可以使用 `.cpp.inc` 后缀。普通声明仍应放在头文件中，普通实现仍应放在 `.cpp` 文件中。

文件名采用大驼峰命名并与其中的主要类型一致，例如 `PayloadCodec.h` 与 `PayloadCodec.cpp`。程序入口 `main.cpp` 保持小写。每个子库有一个 `<目标名>Global.h` 存放导出宏，例如 `HelloKitDocumentGlobal.h`，该文件不对应任何类型，但按目标名命名。

## 大小写

分为三个层次，不可混用：

| 层次 | 写法 | 示例 |
|---|---|---|
| CMake 包名、`project()`、配置模板 | 小写 | `hellokit`、`helloutauConfig.cmake.in` |
| 子库目标名、动态库文件名 | 大驼峰 | `HelloKitDocument`、`HelloUtauEditor.dll` |
| include 命名空间 | 小写模块名 | `<hellokit/Document/...>` |

子库目录采用大驼峰命名，与去掉族前缀后的目标名一致：`lib/Document/` 对应 `HelloKitDocument`，`lib/Editor/` 对应 `HelloUtauEditor`。

## C++ 命名

- 类名及其他类型名使用大驼峰命名。
- 函数名、参数名、变量名和命名空间使用小驼峰命名。表示二元操作左右两侧的 `LHS` 与 `RHS` 是仅有的全大写变量名例外。
- 枚举成员使用大驼峰命名。
- 类的私有数据成员使用 `m_` 前缀。PImpl 中相互关联的实现指针与声明对象指针是例外，使用与 stdcorelib 一致的 `_impl` 与 `_decl`。公有数据成员不使用前缀。
- getter 使用所读取的属性名，例如 `value()`。
- setter 使用 `set` 加属性名，例如 `setValue()`。
- 全局非静态变量使用 `g_` 前缀，全局静态变量使用 `s_` 前缀。应尽量避免引入全局可变状态。
- 命名空间结束处不添加注释。

## PImpl

PImpl 采用 stdcorelib 的写法（`<stdcorelib/pimpl.h>`）。只在实现中使用 stdcorelib 的子库以 `LINKS_PRIVATE` 链接 `stdcorelib::stdcorelib`。

- 声明类在头文件中声明嵌套类 `class Impl;` 与成员 `std::unique_ptr<Impl> _impl;`。
- 实现类声明 `using Decl = <声明类>;`。需要访问声明对象时，持有成员 `Decl *_decl;`，由构造函数的参数 `Decl *decl` 初始化。
- 声明类的成员函数以 `stdc_impl_t;` 取得引用 `impl`，写 `impl.member`，不直接写 `_impl->member`。
- 实现类的成员函数以 `stdc_decl_t;` 取得引用 `decl`，写 `decl.member` 或 `&decl`，不直接写 `_decl->member`。
- 两个宏在 const 成员函数中给出 const 引用。
- 保存到函数返回之后的 lambda（信号连接、定时器等）不捕获 `impl` 或 `decl`，而是捕获 `this`，并在 lambda 体内再写一次宏。
- 实现类的构造函数直接使用参数 `decl`。
- 声明类以外的代码（友元、实现类的嵌套类）经由指针访问，不使用宏。

## 前缀

| 用途 | 前缀 | 示例 |
|---|---|---|
| 仓库级 CMake 选项与变量 | `HELLO_` | `HELLO_BUILD_TESTS` |
| 模块级 CMake 变量 | `HELLOKIT_` / `HELLOUTAU_` | `HELLOKIT_DEVEL` |
| 模块级 CMake 函数 | `hellokit_` / `helloutau_` | `hellokit_add_library` |
| 子库导出宏 | `HELLOKIT_DOCUMENT_` 等 | `HELLOKIT_DOCUMENT_EXPORT` |
| 头文件保护 | 按路径，见下文 | `HELLOKIT_DOCUMENT_PAYLOADCODEC_H` |

**头文件保护**取头文件的路径，全部大写，路径分隔符与扩展名前的点换为下划线，中间各级目录都保留，与 LLVM 编码规范的 Header Guard 一节（`llvm/docs/CodingStandards.rst`）相同。`#endif` 后的注释与保护名一致。

- 公开头文件取 include 路径：`hellokit/include/hellokit/Document/PayloadCodec.h` 为 `HELLOKIT_DOCUMENT_PAYLOADCODEC_H`，`helloutau/include/helloutau/Editor/Dialogs/RegionDialog.h` 为 `HELLOUTAU_EDITOR_DIALOGS_REGIONDIALOG_H`。
- 库内私有头文件取模块名与 `lib/` 之后的路径，文件名保留 `_P`：`helloutau/lib/Editor/Dialogs/AboutDialog_p.h` 为 `HELLOUTAU_EDITOR_DIALOGS_ABOUTDIALOG_P_H`。
- 名为 `private` 的目录不计入，视同其上级目录（作者 2026-10-09 决定）：`hellokit/include/hellokit/Synth/private/ShellSyntax_p.h` 为 `HELLOKIT_SYNTH_SHELLSYNTAX_P_H`。
- 插件的头文件取 `HELLOUTAU_` 与 `plugins/` 之后的路径，`Internal` 计入：`helloutau/plugins/Core/KeymapSettingPage.h` 为 `HELLOUTAU_CORE_KEYMAPSETTINGPAGE_H`，`helloutau/plugins/Core/Internal/CorePlugin.h` 为 `HELLOUTAU_CORE_INTERNAL_COREPLUGIN_H`。
- 测试的辅助头文件取模块名、`TESTS` 与 `tests/auto/` 之后的路径：`hellokit/tests/auto/EditBase/TestSession.h` 为 `HELLOKIT_TESTS_EDITBASE_TESTSESSION_H`。
- 独立程序（`tools/` 与 `tests/manual/` 下的各个程序）的头文件以程序目录名开头，取其后的路径：`hellokit/tools/fswatcher/Backend.h` 为 `FSWATCHER_BACKEND_H`。

模块级函数由 `qm_setup_build_repo_helpers(hellokit)` 生成，**必须显式指定前缀**，因为其默认值为 `PROJECT_NAME`，而子目录中的 `PROJECT_NAME` 已是 `HelloKitDocument`。变量前缀由 `hellokit_init_buildsystem(HELLOKIT)` 显式指定。

子库的导出宏前缀由 `hellokit_add_library(... MACRO_PREFIX HELLOKIT_DOCUMENT)` 显式指定，否则默认值将根据目标名生成为 `HELLOKITDOCUMENT_`。

`<目标名>Global.h` 引用 `<QtCore/QtGlobal>`，导出宏展开为 `Q_DECL_EXPORT` / `Q_DECL_IMPORT`。两个模块都依赖 Qt，使用 Qt 的宏不增加额外依赖；只为一个宏而公开链接 stdcorelib 没有必要。

## 构建与安装选项

| 选项 | 默认 | 作用 |
|---|---|---|
| `HELLO_BUILD_TESTS` | OFF | 构建测试 |
| `HELLO_BUILD_APPLICATION` | ON | 构建编辑器（`helloutau` 模块），关闭时只构建 `hellokit` |
| `HELLO_INSTALL` | ON | 安装 |
| `HELLO_DEVEL` | OFF | 另外安装开发所需的文件：导入库与静态库、头文件、CMake 包、调试符号 |
| `HELLO_INSTALL_TESTING` | OFF | 构建并安装测试的辅助库，供下游项目的测试链接，对应 LLVM 的 `LLVM_INSTALL_GTEST` |

- `HELLO_DEVEL` 关闭时只安装运行所需的文件：程序、动态库、插件及其 `plugin.json`。两个模块把它传给 qmsetup 的 `<模块>_DEVEL`（导入库、头文件、CMake 包）与 `<模块>_INSTALL_PDB`（调试符号）；插件的公开头文件同样只在 `HELLOUTAU_DEVEL` 下安装。
- 调试符号：qmsetup 原先只以 CMake 的 `MSVC` 判断是否安装 PDB。GNU 前端的 clang 面向 MSVC ABI 时（`CMAKE_CXX_SIMULATE_ID` 为 `MSVC`，链接器为 lld-link，生成 PDB）不算 `MSVC`，会被当作 ELF 处理：装出以 `objcopy` 抽取的 `.debug` 而不是 `.pdb`，并对装出的程序与动态库执行 `strip`。qmsetup 已改为同时认模拟的 `MSVC`（2026-09-30），所用的 qmsetup 更新之前，这种工具链下仍是旧的行为。

## Qt

**带 `Q_OBJECT` 的头文件必须出现在目标的 `SOURCES` 中。** AUTOMOC 只处理 `SOURCES` 中列出的文件，而位于 `include/` 下的头文件不会被源文件的 glob 匹配，moc 因此不会生成代码，链接时将缺少 `metaObject`、`qt_metacast`、`qt_metacall`、`staticMetaObject` 四个符号。因此子库必须将头文件一并加入 glob：

```cmake
file(GLOB_RECURSE _src "*.cpp")
file(GLOB_RECURSE _hdr "${CMAKE_CURRENT_SOURCE_DIR}/../../include/helloutau/Editor/*.h")

helloutau_add_library(${PROJECT_NAME} SHARED
    QT_AUTOGEN
    SOURCES ${_src} ${_hdr}
    ...
)
```

qwindowkit 没有这个问题，因为它的头文件与源文件位于同一目录。synthrt 也没有，因为它不使用 Qt。本仓库两个条件都不满足，因此必须显式处理。

**Qt 提供了宏或工具的，使用 Qt 的写法**（作者 2026-10-10 决定），不手写等价的代码。例如禁止拷贝写 `Q_DISABLE_COPY(Type)`，同时禁止移动写 `Q_DISABLE_COPY_MOVE(Type)`，放在类的 private 段末尾，不写成两行 `= delete`。Qt 没有对应宏的情况照常手写，例如保留移动构造而只删除移动赋值。stdutau 不依赖 Qt，不适用此规则。

## 格式与内联

所有改动过的 C++ 文件在提交前使用仓库的 `.clang-format` 格式化。不要手工制造与格式化配置相冲突的对齐或换行。

初始化表达式的类型为指针时，使用 `auto name = ...`，不要写 `auto *name = ...`。`auto` 会自动推导出指针类型，额外的 `*` 不提供信息。

短小且需要暴露定义的函数可以在类内实现，或在头文件的类定义之后实现。不要仅仅为了减少一个 `.cpp` 文件而把较长实现放进公开头文件。

**头文件中实现的函数一律显式写出 `inline` 关键字**，包括类成员函数。语言已隐含 `inline` 的情形同样照写，例如类内定义的成员函数，目的是使头文件中所有实现的写法一致。成员函数在类定义之外实现时，类内的声明与类外的定义都写 `inline`：

```cpp
class Range {
public:
    inline int length() const {
        return m_end - m_begin;
    }

    inline bool contains(int value) const;

private:
    int m_begin = 0;
    int m_end = 0;
};

inline bool Range::contains(int value) const {
    return value >= m_begin && value < m_end;
}
```

## 注释

公开声明使用 LLVM 风格的 `///` 文档注释，不使用 `\brief`。使用 Doxygen 的 `\c` 标识符、`\a` 参数、`\note`、`\warning` 等命令表达结构化含义。

文档注释中指向其他声明或文件的引用写成 `\sa`，放在注释的最后，不写成正文中的「See X.」或「…, see X」。`\sa` 之后只列引用对象，多个以逗号分隔，不写句子；正文须在去掉引用后仍然完整。引用须附带说明，或者引用的是文档中的一节而非一个对象时，写成句子，如「See the section on commands in docs/Editing.md.」「See test_ThreadedSynthRunner.cpp for the remaining coverage gap.」。

```cpp
/// Saves the voice bank. The directories that are not in the tree are neither written nor
/// removed.
///
/// \sa VoiceBankFileSystemState::save(), saveAs()
bool save(DiagnosticList &diagnostics);
```

注释应解释约束、所有权、生命周期以及当前实现必须如此设计的原因。不要用注释记录代码以前的样子或修改历史，这些信息由版本控制保存。

注释使用美式英语。不要用破折号连接从句，也不要用分号代替应有的断句。

## 文体

本节适用于注释、文档、README、帮助文本和诊断消息。

- **采用正式的技术写作文体。** 注释与文档是规范性文本而非叙述。每句陈述一项事实、约束或理由，不写铺垫、感想和修辞。
- **不拟人。** 代码、文件、格式、程序和测试不作为有意志的主语：不写 says、tells、knows、asks、wants、means、cares、decides、promises、is told，也不写「它说」「它知道」「它不认」「它想要」。改用 returns、indicates、records、specifies、reports、detects、requires、rejects，或「返回」「表示」「记录」「规定」「报告」「拒绝」。用户、作者、调用方等真实行为主体可以作主语。
- **使用术语，不用描述性转述。** 写 invalid byte sequence，不写 bytes that do not decode。写 Basic Multilingual Plane、unpaired surrogate、reverse mapping、unrepresentable character，不写 the basic plane、half of one、the way back、what cannot be spelled。没有通用术语时，首次出现给出定义，之后始终沿用同一名称。
- **标题、分组名和列表标签使用名词或名词短语。** 写 Motivation、Behavior、Supported code pages、Rationale，不写 Why、What it does、What it holds、How it works。能用名词表达时，不用 what 引导的名词从句作主语或宾语：写 the requested encoding，不写 what the user asked for。
- **条件用 if，where 只表示处所。** 不写 empty where there is none，写 empty if absent。不写 nothing where the file is missing，写 \c std::nullopt if the file is missing。不用 one 回指前文名词（such a one、the one it wants），直接重复该名词。
- **不使用口语短语。** 不写 whatever else、for good、as it stands、on its own、on the way out、at a glance、there and back、is given up on、the rest of why、and all 等说法，改为准确的书面表达。
- **句子完整。** 不写片段句、逗号粘连句（两个独立分句仅以逗号连接）和反问句。不以 So、And so、Which is why、That is why、Hence 开头叙述因果，改为在同一句中用 because、therefore 表明。不对读者使用第二人称。
- **函数说明以动词开头**（Returns、Decodes、Reads、Rejects）。`\return` 写明每种情况的返回值。布尔查询写 Returns whether …。
- 中文文本同样适用：使用书面语，不用「别」「搞」「就行」「得（表必须）」「啥」「拿来」「反正」「其实」「说白了」「这玩意儿」等口语词。标题不用「为什么」「怎么做」，改用「动机」「设计理由」「实现方式」。「不要」「必须」等规范性祈使句不属于口语，照常使用。

## 其他风格

析构函数不要使用 override 关键字。
头文件内继承的类不要使用 final。

## 返回值

可能不存在结果的函数一律返回 `std::optional<T>`，不要使用「bool 加输出参数」，也不要用某个特定值表示「不存在」。stdutau 已按此规则修改过一轮，本仓库从一开始即遵循此规则。

## 头文件引用

引用块从上到下依次为系统库、标准库、第三方库、项目内被依赖的其他目标和当前目标内的头文件。不同来源的引用块之间留一个空行，同一引用块中的头文件应来自同一个库。不要依赖其他头文件偶然提供的传递引用。

**引用 Qt 的头文件要带模块名**，写 `<QtCore/QByteArray>`、`<QtWidgets/QMainWindow>`，不写 `<QByteArray>`、`<QMainWindow>`。不带模块名的写法依赖构建系统将每个模块的 include 目录加入搜索路径，而目标只链接 QtCore 时该路径未必存在。带上模块名，读者也能立即看出该行依赖哪个模块。

在头文件中引用项目公开头文件时使用完整公共路径：

```cpp
#include <hellokit/Document/PayloadCodec.h>
```

同一子库内部的头文件也使用完整公共路径，不要写成相对路径。子库的 `include/` 目录位于包含路径中，`../../include/hellokit/Document/PayloadCodec.h` 这种写法在目录移动后即会失效。

如果被引用的头文件与当前头文件位于同一目录，并且具有预引入或自动生成等特殊用途，也可以使用双引号直接引用。

在源文件中，同一构建目标内的头文件一律使用双引号直接引用。与源文件同名的公开头文件和 `_p.h` 私有头文件具有最高优先级，必须组成源文件最上方的第一个引用块。系统库、标准库、第三方库和项目内其他目标的头文件依次放在其后。当前目标内的其余头文件具有最低优先级，必须组成最底部的独立引用块。

```cpp
#include "PayloadCodec.h"

#include <array>

#include <stdcorelib/str.h>
#include <stdutau/ustfile.h>

#include "Encoding.h"
```
