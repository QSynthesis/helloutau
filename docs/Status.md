# 项目状态

## 现在有什么

仓库骨架按 synthrt 的形状搭好了，两个模块都能构建。

| 模块 | 状态 |
|---|---|
| `hellokit` | 只有 `Ust/PayloadCodec`，用来验证整条构建链。命名空间 `hu` |
| `helloutau` | 一个 `QLabel`，证明 Qt Widgets 接上了 |

构建链已验证：qmsetup 的 `hellokit_add_library` / `helloutau_add_application`、Qt 6.11、stdcorelib、stdutau、Boost.Test 加 `add_auto_test`、ctest。

`PayloadCodec` 实现了 `_USTH_` 控制音符的载荷编码，base64url 去填充。选这个作为第一块代码不是因为它最重要，是因为它是纯逻辑、不依赖 Qt、而且规则已经被实测钉死了（见 [`claude/utau-ust-preservation.md`](claude/utau-ust-preservation.md)）。

## 依赖怎么来的

- **stdutau**：**不是子模块**，是并排的检出。`third-party/CMakeLists.txt` 直接 `add_subdirectory(../stdutau)`。它正在和本仓库一起改，走子模块指针会让每次改动都要先 push 一轮。等它稳定下来再换成子模块。
- **qmsetup、stdcorelib、Boost**：来自 `D:/GitHub/synthrt/vcpkg`。
- **Qt 6.11.1**：`D:/Qt/6.11.1/msvc2022_64`。

路径都写在 `.vscode/settings.json` 里，那个文件是 gitignore 的。

## 接下来

按依赖顺序，不是按重要性：

1. **编码层**（`hellokit/Support/`）。`docs/note.md` 定的那套策略：工程编码存 `_USTH_`，音源编码存每个目录自己的 `hello-config.json`，插件看 `plugin.json`。转义与还原要配一组往返测试。
2. **工程模型**（`hellokit/Core/`）。`.usth` 的读写，以及和 `.ust` 的互转，包括控制音符的一进一出配平。
3. **音源库模型**（`hellokit/VoiceBank/`）。一个音源多份 `oto.ini`，各自编码。
4. **渲染调度**。`utau::Synth::calc` 出参数，`stdc::Popen` 起进程。线程池的形状参考 QSynthesis-Old 的 `Frontend/Process/`。
5. **插件**。四类，见 `docs/note.md`。

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
