# 项目状态

## 现在有什么

仓库骨架按 synthrt 的形状搭好了，两个模块都能构建。

| 目标 | 状态 |
|---|---|
| `HelloKitDocument` | 只有 `PayloadCodec`，用来验证整条构建链 |
| `HelloUtauWidgets` | 一个装着 `QLabel` 的 `MainWindow`，证明 Qt Widgets 和 moc 接上了 |
| `helloutau` | 薄驱动，只有 `main.cpp` |

构建链已验证：qmsetup 的 `hellokit_add_library` / `helloutau_add_library` / `helloutau_add_application`、Qt 6.11 加 AUTOMOC、stdcorelib、stdutau、Boost.Test 加 `add_auto_test`、ctest。

`PayloadCodec` 实现了 `_USTH_` 控制音符的载荷编码，base64url 去填充。选这个作为第一块代码不是因为它最重要，是因为它是纯逻辑、不依赖 Qt、而且规则已经被实测钉死了（见 [`claude/utau-ust-preservation.md`](claude/utau-ust-preservation.md)）。

## 依赖怎么来的

- **stdcorelib、stdutau**：**都不从 vcpkg 拿，也都不是子模块**，两个都在和本仓库一起改。各自构建安装一份，配置时传 `-Dstdcorelib_DIR=` 和 `-Dstdutau_DIR=`，指向 `<prefix>/lib/cmake/<名字>`。`third-party/Dependencies.cmake` 统一 `find_package`，由根 `CMakeLists.txt` `include()` 进来。Windows 上那里还会把动态库拷进运行输出目录，vcpkg 的 applocal 不再管这两个了。stdutau 现在是静态库，所以没有可拷的 DLL。
- **stdcorelib 只做私有依赖**：子库写 `LINKS_PRIVATE`，公开头文件里的导出宏用 `<QtCore/QtGlobal>` 的 `Q_DECL_EXPORT` / `Q_DECL_IMPORT`。
- **qmsetup、Boost**：来自 `D:/GitHub/synthrt/vcpkg`。
- **Qt 6.11.1**：`D:/Qt/6.11.1/msvc2022_64`。

路径都写在 `.vscode/settings.json` 里，那个文件是 gitignore 的。

## 接下来

阶段划分、每阶段怎么算数、从 QSynthesis 拿什么不拿什么，都在 [`Roadmap.md`](Roadmap.md)。当前处在第一阶段「数据层」的开头，`HelloKitDocument` 里只有 `PayloadCodec`。

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
