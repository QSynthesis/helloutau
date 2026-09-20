# 代码编写原则

本文档规定 helloutau 仓库通用的代码组织与 C++ 编写原则。具体格式以仓库根目录的 `.clang-format` 为准。

与 synthrt 的 `docs/Development.md` 保持一致，两边冲突时以这里为准，但不要为了本仓库的方便去分叉一条规则，先想清楚是不是那边也该改。

## 模块

两个模块，每个模块是**一族库**而不是一个库：

| 模块 | 命名空间 | 产出 | 依赖 |
|---|---|---|---|
| `hellokit/` | `hello::kit` | `HelloKitDocument`、以后的 `HelloKitCore` 等 | Qt Core、stdutau、stdcorelib（私有） |
| `helloutau/` | `hello::daw` | `HelloUtauWidgets` 等，加上 `helloutau` 可执行文件 | Qt Widgets、hellokit |

`hello` 只是外层，代码一律写在第二层里。不要往 `hello` 本身放东西，也不要再开第三层。

**`hellokit` 不链接 QtWidgets。** 界面是应用程序的事，核心逻辑不依赖 GUI 工具包才测得动。

**应用也是库加一个薄驱动**，形状照 lldb 的 `liblldb` 与 `tools/driver`。`tools/driver/main.cpp` 只有入口，其余都在库里，这样应用侧的逻辑同样测得动——可执行文件没法被链进测试二进制，库可以。

## 目录与文件

模块级一个 `include/` 一个 `lib/`，子库在里面各占一格，形状照 synthrt：

```
hellokit/include/hellokit/Document/PayloadCodec.h     ← #include <hellokit/Document/PayloadCodec.h>
hellokit/lib/Document/PayloadCodec.cpp                ← 目标 HelloKitDocument

helloutau/include/helloutau/Widgets/MainWindow.h
helloutau/lib/Widgets/MainWindow.cpp             ← 目标 HelloUtauWidgets
helloutau/plugins/                               ← 编辑界面扩展插件
helloutau/tools/driver/main.cpp                  ← 目标 helloutau
```

**include 的命名空间是模块名，不是目标名。** 写 `<hellokit/Document/PayloadCodec.h>`，不写 `<HelloKitDocument/PayloadCodec.h>`。`HelloKitDocument` 只是产出的动态库文件名。

不用 qmsetup 的 `sync_include`，`include/` 是实打实的目录。

仅供实现使用的私有头文件放在源文件旁边，并使用 `_p.h` 后缀。私有头文件会增加实现之间的耦合，应尽量少用。

仅供多个实现文件复用且不独立编译的实现片段可以使用 `.cpp.inc` 后缀。普通声明仍应放在头文件中，普通实现仍应放在 `.cpp` 文件中。

文件名采用大驼峰命名并与其中的主要类型一致，例如 `PayloadCodec.h` 与 `PayloadCodec.cpp`。程序入口 `main.cpp` 保持小写。每个子库有一个 `<目标名>Global.h` 放导出宏，例如 `HelloKitDocumentGlobal.h`，它不对应类型但跟着目标名走。

## 大小写

三层，别混：

| 层 | 写法 | 例 |
|---|---|---|
| CMake 包名、`project()`、配置模板 | 小写 | `hellokit`、`helloutauConfig.cmake.in` |
| 子库目标名、动态库文件名 | 大驼峰 | `HelloKitDocument`、`HelloUtauWidgets.dll` |
| include 命名空间 | 小写模块名 | `<hellokit/Document/...>` |

子库目录用大驼峰并与目标名去掉族前缀后一致：`lib/Document/` 对 `HelloKitDocument`，`lib/Widgets/` 对 `HelloUtauWidgets`。

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

| 用途 | 前缀 | 例 |
|---|---|---|
| 仓库级 CMake 选项与变量 | `HELLO_` | `HELLO_BUILD_TESTS` |
| 模块级 CMake 变量 | `HELLOKIT_` / `HELLOUTAU_` | `HELLOKIT_DEVEL` |
| 模块级 CMake 函数 | `hellokit_` / `helloutau_` | `hellokit_add_library` |
| 子库导出宏 | `HELLOKIT_DOCUMENT_` 等 | `HELLOKIT_DOCUMENT_EXPORT` |
| 头文件保护 | 按 include 路径 | `HELLOKIT_DOCUMENT_PAYLOADCODEC_H` |

模块级的函数由 `qm_setup_build_repo_helpers(hellokit)` 生成，**必须显式给前缀**——它默认取 `PROJECT_NAME`，而子目录里 `PROJECT_NAME` 已经是 `HelloKitDocument` 了。变量前缀由 `hellokit_init_buildsystem(HELLOKIT)` 显式给。

子库的导出宏前缀由 `hellokit_add_library(... MACRO_PREFIX HELLOKIT_DOCUMENT)` 显式给，默认值会跟着目标名走成 `HELLOKITDOCUMENT_`。

`<目标名>Global.h` 引 `<QtCore/QtGlobal>`，用 `Q_DECL_EXPORT` / `Q_DECL_IMPORT` 展开。两个模块都依赖 Qt，而 stdcorelib 是私有依赖，不出现在公开头文件里。

## Qt

**带 `Q_OBJECT` 的头文件必须出现在目标的 `SOURCES` 里。** AUTOMOC 只扫 `SOURCES` 列出的文件，而头文件放在 `include/` 下不会被源文件的 glob 捞到，于是 moc 不生成，链接时缺 `metaObject`、`qt_metacast`、`qt_metacall`、`staticMetaObject` 四个符号。所以子库要把头文件也 glob 进去：

```cmake
file(GLOB_RECURSE _src "*.cpp")
file(GLOB_RECURSE _hdr "${CMAKE_CURRENT_SOURCE_DIR}/../../include/helloutau/Widgets/*.h")

helloutau_add_library(${PROJECT_NAME} SHARED
    QT_AUTOGEN
    SOURCES ${_src} ${_hdr}
    ...
)
```

qwindowkit 没有这个问题是因为它把头文件和源文件放在一起，synthrt 没有是因为它根本不用 Qt。我们两样都不占，所以要自己记着。

## 格式与内联

所有改动过的 C++ 文件在提交前使用仓库的 `.clang-format` 格式化。不要手工制造与格式化配置相冲突的对齐或换行。

初始化表达式的类型为指针时，使用 `auto name = ...`，不要写 `auto *name = ...`。`auto` 会自动推导出指针类型，额外的 `*` 不提供信息。

短小且需要暴露定义的函数可以在类内实现，或在头文件的类定义之后使用 `inline` 实现。不要仅仅为了减少一个 `.cpp` 文件而把较长实现放进公开头文件。

## 注释

公开声明使用 LLVM 风格的 `///` 文档注释，不使用 `\brief`。使用 Doxygen 的 `\c` 标识符、`\a` 参数、`\note`、`\warning` 等命令表达结构化含义。

注释应解释约束、所有权、生命周期以及当前实现必须如此设计的原因。不要用注释记录代码以前的样子或修改历史，这些信息由版本控制保存。

注释使用美式英语。不要用破折号连接从句，也不要用分号代替应有的断句。

## 其他风格

析构函数不要使用 override 关键字。
头文件内继承的类不要使用 final。

## 返回值

读不到就是没有的地方一律返回 `std::optional<T>`，不要用「bool 加出参」，也不要拿某个特定值当作「没有」。stdutau 已经按这条改过一轮，本仓库从一开始就这样写。

## 头文件引用

引用块从上到下依次为系统库、标准库、第三方库、项目内被依赖的其他目标和当前目标内的头文件。不同来源的引用块之间留一个空行，同一引用块中的头文件应来自同一个库。不要依赖其他头文件偶然提供的传递引用。

在头文件中引用项目公开头文件时使用完整公共路径：

```cpp
#include <hellokit/Document/PayloadCodec.h>
```

同一子库内部的头文件也用完整公共路径，不要写成相对路径。子库的 `include/` 那一层在包含路径里，`../../include/hellokit/Document/PayloadCodec.h` 这种写法一旦目录挪动就断。

如果被引用的头文件与当前头文件位于同一目录，并且具有预引入或自动生成等特殊用途，也可以使用双引号直接引用。

在源文件中，同一构建目标内的头文件一律使用双引号直接引用。与源文件同名的公开头文件和 `_p.h` 私有头文件具有最高优先级，必须组成源文件最上方的第一个引用块。系统库、标准库、第三方库和项目内其他目标的头文件依次放在其后。当前目标内的其余头文件具有最低优先级，必须组成最底部的独立引用块。

```cpp
#include "PayloadCodec.h"

#include <array>

#include <stdcorelib/str.h>
#include <stdutau/ustfile.h>

#include "Encoding.h"
```
