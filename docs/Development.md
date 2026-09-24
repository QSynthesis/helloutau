# 代码编写原则

本文档规定 helloutau 仓库通用的代码组织与 C++ 编写原则。具体格式以仓库根目录的 `.clang-format` 为准。

本文档与 synthrt 的 `docs/Development.md` 保持一致。两者冲突时以本文档为准，但不应为了本仓库的便利而分叉规则，应先确认 synthrt 的规则是否也需要修改。

## 模块

共两个模块，每个模块是**一组库**而非单个库：

| 模块 | 命名空间 | 产出 | 依赖 |
|---|---|---|---|
| `hellokit/` | `hello::kit` | `HelloKitDocument`、今后的 `HelloKitCore` 等 | Qt Core、stdutau、stdcorelib（私有）、substate（私有，仅 `HelloKitEditBase` 与 `HelloKitEdit`） |
| `helloutau/` | `hello::daw` | `HelloUtauWidgets` 等，以及 `HelloUtau` 可执行文件 | Qt Widgets、hellokit |

`hello` 仅作为外层命名空间，代码一律位于第二层。不要在 `hello` 中直接声明内容，也不要再增加第三层。

**例外：`HelloKitEditBase` 位于第三层命名空间 `hello::kit::edit`。** 它是编辑层与文档无关的通用部分，稳定后将移入 substate，第三层命名空间使 `Slot`、`Range`、`Change`、`NodeRef` 等通用的名字在此之前不占用 `hello::kit`。移走后此例外取消。见 [`Editing.md`](Editing.md) 的「通用层与文档层」。

**`hellokit` 不链接 QtWidgets。** 界面属于应用程序，核心逻辑不依赖 GUI 工具包才便于测试。

**应用同样由库和一个薄驱动组成**，结构参照 lldb 的 `liblldb` 与 `tools/driver`。`tools/driver/main.cpp` 只包含入口，其余逻辑均位于库中，因此应用侧的逻辑同样可以测试：可执行文件无法链接进测试程序，而库可以。

## 目录与文件

每个模块包含一个 `include/` 和一个 `lib/`，各子库在其中各占一个目录，结构参照 synthrt：

```
hellokit/include/hellokit/Document/PayloadCodec.h     ← #include <hellokit/Document/PayloadCodec.h>
hellokit/lib/Document/PayloadCodec.cpp                ← 目标 HelloKitDocument

helloutau/include/helloutau/Widgets/MainWindow.h
helloutau/lib/Widgets/MainWindow.cpp             ← 目标 HelloUtauWidgets
helloutau/plugins/                               ← 编辑界面扩展插件
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
| 子库目标名、动态库文件名 | 大驼峰 | `HelloKitDocument`、`HelloUtauWidgets.dll` |
| include 命名空间 | 小写模块名 | `<hellokit/Document/...>` |

子库目录采用大驼峰命名，与去掉族前缀后的目标名一致：`lib/Document/` 对应 `HelloKitDocument`，`lib/Widgets/` 对应 `HelloUtauWidgets`。

## C++ 命名

- 类名及其他类型名使用大驼峰命名。
- 函数名、参数名、变量名和命名空间使用小驼峰命名。表示二元操作左右两侧的 `LHS` 与 `RHS` 是仅有的全大写变量名例外。
- 枚举成员使用大驼峰命名。
- 类的私有数据成员使用 `m_` 前缀。PImpl 中相互关联的实现指针与声明对象指针是例外，使用与 stdcorelib 一致的 `_impl` 与 `_decl`。公有数据成员不使用前缀。
- getter 使用所读取的属性名，例如 `value()`。
- setter 使用 `set` 加属性名，例如 `setValue()`。
- 全局非静态变量使用 `g_` 前缀，全局静态变量使用 `s_` 前缀。应尽量避免引入全局可变状态。
- 命名空间结束处不添加注释。

## 前缀

| 用途 | 前缀 | 示例 |
|---|---|---|
| 仓库级 CMake 选项与变量 | `HELLO_` | `HELLO_BUILD_TESTS` |
| 模块级 CMake 变量 | `HELLOKIT_` / `HELLOUTAU_` | `HELLOKIT_DEVEL` |
| 模块级 CMake 函数 | `hellokit_` / `helloutau_` | `hellokit_add_library` |
| 子库导出宏 | `HELLOKIT_DOCUMENT_` 等 | `HELLOKIT_DOCUMENT_EXPORT` |
| 头文件保护 | 按 include 路径 | `HELLOKIT_DOCUMENT_PAYLOADCODEC_H` |

模块级函数由 `qm_setup_build_repo_helpers(hellokit)` 生成，**必须显式指定前缀**，因为其默认值为 `PROJECT_NAME`，而子目录中的 `PROJECT_NAME` 已是 `HelloKitDocument`。变量前缀由 `hellokit_init_buildsystem(HELLOKIT)` 显式指定。

子库的导出宏前缀由 `hellokit_add_library(... MACRO_PREFIX HELLOKIT_DOCUMENT)` 显式指定，否则默认值将根据目标名生成为 `HELLOKITDOCUMENT_`。

`<目标名>Global.h` 引用 `<QtCore/QtGlobal>`，导出宏展开为 `Q_DECL_EXPORT` / `Q_DECL_IMPORT`。两个模块都依赖 Qt，而 stdcorelib 是私有依赖，不出现在公开头文件中。

## Qt

**带 `Q_OBJECT` 的头文件必须出现在目标的 `SOURCES` 中。** AUTOMOC 只处理 `SOURCES` 中列出的文件，而位于 `include/` 下的头文件不会被源文件的 glob 匹配，moc 因此不会生成代码，链接时将缺少 `metaObject`、`qt_metacast`、`qt_metacall`、`staticMetaObject` 四个符号。因此子库必须将头文件一并加入 glob：

```cmake
file(GLOB_RECURSE _src "*.cpp")
file(GLOB_RECURSE _hdr "${CMAKE_CURRENT_SOURCE_DIR}/../../include/helloutau/Widgets/*.h")

helloutau_add_library(${PROJECT_NAME} SHARED
    QT_AUTOGEN
    SOURCES ${_src} ${_hdr}
    ...
)
```

qwindowkit 没有这个问题，因为它的头文件与源文件位于同一目录。synthrt 也没有，因为它不使用 Qt。本仓库两个条件都不满足，因此必须显式处理。

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
