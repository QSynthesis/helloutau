# 原生插件与扩展点的计划

本文档是 HelloUtau 原生插件机制的计划，写于 2026-09-30，尚未实现。作者已于同日决定全部待定事项（见末尾），[`note.md`](note.md) 的插件一节已按决定改写，插件的定义仍以该文档为唯一权威。

## 动机

[`note.md`](note.md) 原先规定了六种插件。除原版 UTAU 插件外，其余五种各有一个本仓库的插件类（`RangeEditPlugin`、`EditorExtensionPlugin`、`VoiceBankPlugin`、`InterchangePlugin`、`FrequencyFormatPlugin`），已写出的是后两个，都只是「把若干驱动交给宿主」的接口，且都依赖 Qt 的插件机制（`Q_DECLARE_INTERFACE`）。应用至今没有载入任何插件。

作者 2026-09-30 提出：以 stdcorelib 的插件库管理插件的载入与生命周期，本仓库**不再定义插件类，只提供各扩展点的注册接口**。

## 调研

### 两个候选

| 方面 | ExtensionSystem（`stdware/ExtensionSystem`，本机 `D:\GitHub\ExtensionSystem`） | stdcorelib.plugin（`stdware/stdcorelib.plugin`，本机 `D:\GitHub\stdcorelib.plugin`） |
|---|---|---|
| 来源 | Qt Creator 3.6 插件框架的移植，最后提交 2025-02，DiffScope 经 choruskit 使用 | 同一组织新写的库，最后提交 2026-09-18；以 ExtensionSystem 为参考，只借用与 Qt 无关的依赖解析、加载顺序与生命周期顺序 |
| 许可证 | LGPL 2.1/3（Qt Company）；本仓库为 Apache 2.0，须作为单独的动态库链接并遵守 LGPL | MIT |
| 依赖 | Qt Core 与 Widgets，`hellokit` 不能使用 | 只依赖 stdcorelib，不依赖 Qt |
| 插件交出功能的方式 | 全局对象池 `addObject` / `getObject<T>`，无类型、依赖顺序 | 没有对象池，由宿主提供注册接口 |
| 管理者 | `PluginManager`，全为静态函数的进程单例 | `stdc::pluginsystem::PluginSystem`，普通对象 |
| 元数据 | `Q_PLUGIN_METADATA`，经 moc | 嵌入动态库的 JSON，不执行插件代码即可读取 |
| 设置 | `QSettings` | `PluginSettings`，全局与用户两份，JSON 值，另有应用自定的 `userData` |
| 界面 | 插件列表、详情、错误视图（`.ui`） | 无 |
| 稳定性 | 代码停在 Qt Creator 3.6 | 三个平台与多种编译器的测试齐全；**公开 API 在首个真实扩展点迁移前仍是候选** |

stdcorelib.plugin 的交接记录（其仓库 `.cache/codex/2026-08-21-1722-stdcorelib-plugin-handoff.md`）明确不沿用 ExtensionSystem 的对象池（无类型的全局服务定位器）、Aggregation、`OptionsParser` 与界面。

**结论：采用 stdcorelib.plugin。** 它与「只提供注册接口」的设想一致，许可证与分层都与本仓库相容，符合 CLAUDE.md「基础设施优先使用 stdcorelib」。代价是 API 可能随迁移反馈调整，HelloUtau 将是它的首个真实使用者之一；与 stdutau、substate 相同，发现不足时直接修改该库。

### stdcorelib.plugin 提供的部分

- `stdc::plugin`：插件是某个 IID 接口的实现，以 `STDC_EXPORT_PLUGIN` 导出，IID 与元数据嵌在库中；`PluginFactory` 按目录发现。
- `stdc::pluginsystem::PluginSystem`：只接收一个 IID；元数据保留 `id`、`displayName`、`description`、`version`、`compatVersion`、`enabledByDefault`、`dependencies`，其余字段归宿主；依赖解析、按依赖顺序载入并调用 `initialize()`、逆序调用 `pluginsInitialized()` 与 `aboutToShutdown()` 并卸载；全局与用户两份启用设置；宿主的载入判定（`setPluginLoadPredicate()`）；Flat、Bundle 与自定义三种目录布局。
- 插件实现 `stdc::pluginsystem::IPlugin` 的三个钩子：`initialize()`、`pluginsInitialized()`、`aboutToShutdown()`。
- stdcorelib 另有 `stdc::DynamicRegistry`：运行时增删、线程安全、可监听条目的增删，由使用方创建并持有（stdcorelib `1c45501` 起，此前为每个进程一份）；以 `std::map` 或 `stdc::linked_map` 保存，后者保持登记顺序；`DynamicRegistry::Registration` 在其生命周期内登记一项。

## 设计

### 一种原生插件，若干扩展点

- 原生插件只有一种：实现 `stdc::pluginsystem::IPlugin`，以 `STDC_EXPORT_PLUGIN` 导出。**本仓库不定义任何插件的基类或接口类。**
- IID 为 `org.OpenVPI.HelloUtau.Plugin`（作者 2026-09-30 决定，同 DiffScope 的 `org.OpenVPI.DiffScope.Plugin`，QSynthesis 亦属 OpenVPI）。C++ 中为 `hello::daw::AppLoader::pluginIid`，本仓库插件的 CMake 中为 `HELLOUTAU_PLUGIN_IID`（`helloutau/conf.cmake`），两者由 `test_AppLoader` 以按后者构建的插件经前者载入来核对。
- 插件 ID 参照 DiffScope（`org.diffscope.core`）：`org.helloutau.<名称>`，全小写。
- 插件在 `initialize()` 中经本仓库的注册接口登记它提供的东西。note.md 中原来的五种 C++ 插件改为五个**扩展点**：频率表格式、格式转换驱动（读写器）、选区编辑、编辑界面扩展、音源批量操作。
- **原版 UTAU 插件的支持本身也是一个原生插件**（作者 2026-09-30 决定）：随 HelloUtau 提供，名为 ClassicPluginHost（作者定名）。它在 `initialize()` 中发现 UTAU 插件文件夹（`plugin.txt`，有 `plugin.json` 时以其为准，编码规则见 note.md），把每个 UTAU 插件作为一项命令加入「工具 → Classic Plugins」菜单，运行时写出临时文件、启动可执行文件、读回结果。停用它即不再提供 UTAU 插件，应用本身不含 UTAU 插件的代码。临时文件的写出与结果的解析在 stdutau 中，其余全部在插件中，库中不为它新开模块；「选区编辑」的通用扩展点等出现第二个使用者时再提炼（作者 2026-09-30 定，见 [`ClassicPluginHost.md`](ClassicPluginHost.md)）。
- 删除 `FrequencyFormatPlugin` 与 `InterchangePlugin` 两个接口（均已删）。

### 注册接口

- 各扩展点的登记形状一致（作者 2026-09-30 要求统一）：

  | 部分 | 动作（HelloUtauWidgets） | 设置页（HelloUtauWidgets） | 频率表格式（HelloKitVoiceBank） | 格式转换驱动（HelloKitInterchange） |
  |---|---|---|---|---|
  | 被登记的对象 | `ActionContribution` | 创建页面的函数 `Factory`（`SettingPage *(QObject *host)`）与页面的位置 | `FrequencyFormat` | `InterchangeReader` 或 `InterchangeWriter` |
  | 登记对象 | `ActionRegistration(ActionContributionRegistry *, std::unique_ptr<ActionContribution>)`，`contribution()` | `SettingPageRegistration(SettingPageRegistry *, Factory, parent, before)`，`addTo()` | `FrequencyFormatRegistration(std::unique_ptr<FrequencyFormat>)`，`format()` | `InterchangeRegistration`，两个构造函数，`reader()` / `writer()` |
  | 登记所在的列表 | `ActionContributionRegistry`（公开，`Editor` 创建并持有） | `SettingPageRegistry`（公开，`Editor` 创建并持有） | `FrequencyFormatRegistrations_p.h`（进程级，私有） | `InterchangeRegistrations_p.h`（进程级，私有） |
  | 使用方 | 持有列表的 `Editor`，经列表的信号得知变化 | 同左 | 每个 `FrequencyFormatRegistry` 监听列表 | 每个 `InterchangeRegistry` 监听列表 |
  | 内置的登记 | `BuiltinActions`（Core 为其编辑器创建，是编辑器的子对象） | 无，编辑器与 Core 的页面直接加入目录 | `BuiltinFrequencyFormats`（FrequencyEditor 持有） | `BuiltinInterchangeDrivers`（Interchange 持有） |

- **动作与设置页的列表由 `Editor` 持有，不是进程级的**（作者 2026-10-08 决定）：没有全局状态，测试直接构造编辑器或列表即可。Core 插件创建编辑器后以 `AppLoader::setEditor()` 交给加载器，关闭时清除；其他插件依赖 Core，在 `initialize()` 中经 `AppLoader::editor()` 取得编辑器，再向其列表登记，取不到时初始化失败。登记对象以 `QPointer` 指向列表，编辑器先销毁时登记的析构不做任何事；编辑器析构时先删除两个列表，此后的注销不再通知编辑器。频率表格式与格式转换驱动仍是进程级列表，两种形状留待抽出注册表模板时统一。
- **设置页登记的是创建页面的函数而非页面**（作者 2026-10-08 同意）：页面属于某个 `Editor` 的设置目录，每个 `Editor` 须有自己的一份。`Editor` 随登记创建页面，随注销以 `SettingCatalog::removePage()` 删除；页面执行插件库中的代码，必须在插件库卸载之前删除。`parent` 为空或找不到时页面放在顶层，`before` 同 `SettingCatalog::addPage()`。工厂收到的 `host` 是持有目录的对象，即 `Editor`；需要 `AppSettings` 的页面以 `qobject_cast<Editor *>` 取得编辑器。

- 登记对象持有被登记的对象，构造时加入列表，析构时移除。列表按登记顺序保存，只在应用的线程上使用，不用 `stdc::DynamicRegistry`：后者按名称排序，表达不了登记顺序。进程级的列表在所属的子库中，子库是动态库，因此每个进程只有一份。
- 插件把登记对象作为自己的成员，在 `initialize()` 中创建，在 `aboutToShutdown()` 中销毁。不能等插件实例析构：实例是插件库中的静态对象，随库卸载才析构，那时 `Editor` 已销毁，且析构发生在卸载库的过程中。
- 内置的格式与驱动经同一途径登记，与插件不分主次，保持 `InterchangeRegistry` 与 `FrequencyFormatRegistry` 已有的原则。
- 应用与测试持有的注册表（`FrequencyFormatRegistry` 等）**仍然不是全局单例**，内容来自进程级列表。测试只登记自己需要的内容，登记对象随测试结束而销毁。
- 注册表监听列表的增删，因此插件在运行中才登记或注销也能反映到界面，例如音源窗口的「F0」下拉框。
- 登记对象在 `initialize()` 中创建（作者 2026-09-30 决定）：生命周期明确，可带运行时参数，例如 UTAU 插件支持插件按发现结果登记的各项。未采用的做法是以静态对象在插件载入时自动登记：名称须是字面量、登记不加锁，且公开头文件中会出现 stdcorelib 的类型。

### 加载器与 Core 插件

作者 2026-09-30 决定，参照 DiffScope 的加载器（choruskit 的 `CkLoader`：程序的 `main` 只配置并调用 `run()`，核心功能在 Core 插件中），但不像 DiffScope 把编辑器核心全放进 Core：编辑器的实现已在 hellokit 与 helloutau 的库中，Core 插件只接管原来程序入口的工作。

- **程序只是加载器。** `helloutau.exe` 的 `main` 设置应用名，构造 `hello::daw::AppLoader` 并调用 `run()`。
- **`AppLoader`**（HelloUtauEditor）：持有 `PluginSystem`（目录布局）。命令行中的 `--plugin-path <目录>` 追加搜索目录，其余参数为文件，交给 Core 插件。`run()` 载入插件；Core 插件不存在、有错误或停用时报告原因并退出；否则运行事件循环，结束后关闭插件。其他插件的错误写入日志，并显示在设置的「Plugins」页。同一时刻只有一个加载器，插件经 `AppLoader::instance()` 取得它。
- **Qt 的插件**（作者 2026-10-01 要求，不用 `qt.conf`）：安装后的布局为 `bin`（程序与各库）、`lib/plugins/helloutau`（本程序的插件）、`lib/plugins/Qt`（Qt 的插件）。程序入口在创建 `QApplication` 之前调用 `AppLoader::addQtPluginPaths()`，因为 Qt 在创建应用对象时载入平台插件。它以 stdcorelib 的 `application_directory()` 取得程序所在目录，把存在的 `bin/plugins`（用户自行复制 Qt 插件的位置）与 `lib/plugins/Qt` 依次加在 Qt 默认路径之前；Qt 6 保留创建应用对象之前加入的路径，并自行搜索程序所在目录，因此 Qt 插件目录直接散在 `bin` 中也能找到。Windows 的打包脚本不在仓库中（`.cache/claude/tools/package_windows.ps1`）：`cmake --install` 之后补上构建时复制到程序旁的依赖 DLL，以 windeployqt 的 `--plugindir` 部署 Qt，复制 VC 运行库的 DLL，并删除头文件、CMake 包与导入库。
- **Core 插件**（ID `org.helloutau.core`，目录 `Core`，目标 `CorePlugin`，插件类在 `Internal` 中）：`initialize()` 创建 `Editor`，向其登记编辑器的动作清单（`BuiltinActions`，见下文「编辑界面扩展：动作与命令」），并以 `AppLoader::setEditor()` 交给加载器供其他插件取用；`pluginsInitialized()` 打开命令行中的文件，没有则新建工程；`aboutToShutdown()` 销毁 `Editor` 及其窗口。`pluginsInitialized()` 按依赖的逆序调用，依赖 Core 的插件先于它完成，因此窗口打开时各插件都已登记完毕。

### 目录

作者 2026-09-30 决定的布局：

```
bin/
    helloutau.exe
    （各动态库）
lib/plugins/helloutau/
    Core/
    ClassicPluginHost/
    FrequencyEditor/
    Interchange/
```

macOS 的 bundle 中为 `HelloUtau.app/Contents/MacOS`（程序）与 `HelloUtau.app/Contents/Plugins/<插件>`。

- 目录布局（stdcorelib.plugin 的 `Bundle`）：每个插件占搜索目录下的一个子目录，其中有插件的库与一个元数据 JSON：根字段 `name` 给出库的平台无关名（`name` 为 `vs4ufrq` 时可对应 `vs4ufrq.dll`、`libvs4ufrq.so`、`libvs4ufrq.dylib`），其余为 `id`、`displayName`、`description`、`version`、`dependencies` 与宿主字段。IID 仍以 `stdc_add_plugin_metadata()` 嵌在库中。插件的其他文件（翻译、图标、数据）放在同一子目录中。
- 内置插件的搜索目录（`AppLoader::builtinPluginPath()`）为程序所在目录上一级的 `lib/plugins/helloutau`；macOS 打包为 bundle 时沿用 qmsetup 的布局，程序在 `Contents/MacOS`，插件在 `Contents/Plugins`。这一相对路径不写在 C++ 中，由 HelloUtauEditor 的 CMake 从 qmsetup 的运行目录与插件目录算出，并核对构建目录与安装目录一致。目前尚未打包为 bundle，测试程序也不在 bundle 中。用户安装插件的目录将来在「Plugins」页一并加入，都与 UTAU 插件的目录分开。
- 本仓库的插件以 `helloutau_add_native_plugin()`（`helloutau/plugins/CMakeLists.txt`）构建：输出到各自的子目录，嵌入 IID，并在构建时写出 `plugin.json`（`DEPENDENCIES` 写入必需依赖）。只供测试的插件以 `DIRECTORY` 构建到测试自己的目录，不安装。
- **翻译**（作者 2026-10-01 定）：各库（两个模块的全部子库，含动作清单经 AEC 生成的文字）共用一份 `helloutau/translations/helloutau_<语言>.ts`，嵌入 HelloUtauEditor 的资源 `:/helloutau/translations`；每个插件一份 `plugins/<插件>/translations/<插件>_<语言>.ts`，嵌入插件自身的资源 `:/helloutau/plugins/<插件>/translations`。`.ts` 纳入版本库，`qt_add_translations()` 在构建时生成 `.qm`；`update_translations` 目标以 lupdate 从源文件（含 AEC 生成的源文件，须先构建）更新 `.ts`。以后改为外部文件时，库的译文放 `share/helloutau`，插件的放各自目录。命名空间作用域中名为 `tr` 的辅助函数会让 lupdate 记错上下文，须改用 `QT_TRANSLATE_NOOP` 标记（见 `ThemeTypes.cpp`）。
- **插件同时是库**（作者 2026-09-30 要求）：与子库一样导出目标并安装头文件，供其他插件在它之上构建。公开头文件与源文件同在 `helloutau/plugins/<插件目录>/`，插件目标以 `helloutau/plugins` 为公开的包含目录，以 `<插件目录>/<头文件>` 引用；安装到 `include/helloutau/plugins/<插件目录>/`，安装后的包含目录指向 `include/helloutau/plugins`。导出宏头文件为 `<目标名>Global.h`，有公开的类时才添加。
- **插件类不导出**（作者 2026-09-30，同 DiffScope 的 `coreplugin/internal`）：插件类与其余实现放在插件目录的 `Internal` 中，不导出，不安装。
- **插件的测试**位于 `helloutau/tests/auto/plugins/<插件>/`，只为插件中库一级的内容（工具类等）而写，不为菜单布局之类会随时调整的东西写测试（作者 2026-09-30）。只供测试的插件（如 TestAction）也放在那里。
- 插件目录关闭 vcpkg 的 applocal：插件链接的库在载入插件前已由程序载入，applocal 只会把 vcpkg 安装树中的库复制到插件旁，其中包括与程序所用版本不同的 stdcorelib（实际发生过）。
- 元数据文件名：沿用该库默认的 `plugin.json`，以 `PluginSystem(iid, PluginSystem::Bundle)` 直接构造（作者 2026-09-30 决定）。它与 UTAU 插件文件夹的 `plugin.json` 同名，但两种目录不会互相搜索，库中的 IID 也能区分原生插件，不会误读。
- 兼容性：C++ 插件须与宿主以同一编译器、同一 Qt 与 hellokit 版本构建。元数据加一个宿主字段（如 `helloutau` 的版本范围），由载入判定检查；判定也用于平台限制（如只在 Windows 可用的 vs4ufrq 格式插件）。
- 设置：应用数据目录中的两个 JSON 文件（作者 2026-09-30 定；Windows 上为 `%APPDATA%\OpenVPI\HelloUtau\`，组织名 `OpenVPI`），应用与插件分开，一方写坏不牵连另一方，插件一份也与将来随安装提供的全局一份同格式：
  - `settings.json`：应用的设置（`AppSettings`），分组存放 `engines`、`playback`、`files`、`commandPalette`，也可经 `value()` / `setValue()` 以 `a/b/c` 形式的键读写任意一层。Core 插件把 `AppLoader` 的这一份交给 `Editor`。
  - `plugins.json`：严格为 stdcorelib.plugin `PluginSettings` 的格式：用户启用或停用的插件 `enabledPlugins` / `disabledPlugins`，以及各插件自己的值 `userData/<插件 ID>`。`AppLoader` 读写它，载入插件前把它交给 `PluginSystem` 的用户一级；插件经 `AppLoader::pluginValue(id, key)` / `setPluginValue()` 以相对于自己那一组的 `a/b/c` 键读写。
  - `AppLoader` 的 `--settings <目录>` 另指定两者所在的目录，测试用它。内部存储用 stdcorelib 的 JSON（值可就地修改，`SettingsJson`），公开接口用 `QJsonValue`，stdcorelib 仍是私有依赖。修改后等事件循环运行时重写整个文件（`SettingsFile`，同一轮循环的修改合为一次写），`AppSettings::sync()`、`AppLoader::syncSettings()` 与析构时立即写出未写的修改；多开时后写的覆盖先写的，以后再做独占。
  - 随安装提供的全局一份尚未实现。
- **Core 插件的设置页**：「Keymap」「Menus and Toolbars」（`core.Keymap`、`core.MenusAndToolbars`，作者 2026-10-01 同意）与「Plugins」（`core.Plugins`，作者 2026-09-30 同意的方案）。Core 插件在创建 `Editor` 后经 `addCoreSettingPages()` 加入，位置见 [`Widgets.md`](Widgets.md)「页面的归属」。三页与该函数都在插件根目录导出（`COREPLUGIN_EXPORT`），供 `tests/auto/plugins/Core` 测试，插件类仍在 `Internal` 中（作者 2026-10-01 要求三页放在一起）。Plugins 页只在经加载器启动时加入，排在 Rendering 之前，测试中直接构造的 `Editor` 没有它。
  - 数据来自 `AppLoader::plugins()`：每个找到的插件一项 `PluginInfo`（ID、显示名、版本、库文件、依赖及是否可选、本次运行的状态「运行中 / 已停用 / 出错 / 未载入」、错误、元数据是否启用、本次是否启用），公开接口不含 stdcorelib 的类型。`errors()` 由它筛出。
  - 列表为「名称（勾选框）、版本、状态」三列，出错的插件在状态列带警告图标，提示为错误全文；选中一项时下方显示 ID、库文件、所依赖的插件与依赖它的插件（标出可选依赖）、错误。
  - 勾选表示下次启动是否启用，经 `AppLoader::pluginEnabled()` / `setPluginEnabled()` 写入 `plugins.json` 的 `enabledPlugins` / `disabledPlugins`，本次运行的插件不变；与元数据相同的选择不写入，文件中只留用户改过的插件。有插件的勾选与本次运行不同时，页首提示重启。
  - Core 插件的勾选框不能取消。停用被依赖的插件不加阻止，依赖它的插件在下次启动时显示依赖错误。
- **关闭顺序**：插件登记的对象，代码都在插件的库中，必须在卸载前销毁。Core 插件在 `aboutToShutdown()` 中销毁 `Editor`，依赖 Core 的插件的 `aboutToShutdown()` 在它之前调用，各库都在此后才卸载。插件的实例是库中的静态对象，随库卸载而析构，因此窗口等 Qt 对象不能留到那时。
- **插件交给宿主的数据不能指向插件库的静态存储**：`QStringLiteral` 的文本就在库中，库卸载后仍被宿主持有的这类字符串即成悬空（`test_AppLoader` 的测试插件遇到过）。交给宿主、可能在卸载后仍被使用的字符串须是分配的副本。

### 编辑界面扩展：动作与命令

作者 2026-09-30 要求插件能注册自己的动作（命令），这是「编辑界面扩展」扩展点的第一部分。

- **qactionkit 已有的部分**：多份清单合并，插件的清单以 `<insertions>` 插入宿主的菜单（`anchor`、`priority`）。为插件卸载补充了 `ActionRegistry::removeExtension()`（qactionkit `46bce9b`），移除后立即重新计算，registry 中不再有指向该清单的视图。
- **`ActionContribution`**（HelloUtauWidgets）：每种窗口至多一份 AEC 编译的清单，由 `extension(const QString &windowKind)` 给出，编辑器把它登记到该种窗口的 registry（每种窗口一个 registry，见 [`Widgets.md`](Widgets.md)「Keymap 页」），加上为窗口创建动作的 `addActions(QWidget *, context)`。窗口种类以宿主定义的名称区分，编辑器的为 `Editor::projectWindowName` 与 `Editor::voiceBankWindowName`，与快捷键、布局文件的分节名相同；需要某种窗口接口的贡献自行 `qobject_cast`，忽略其他种类（作者 2026-10-08 决定：动作与设置页的登记属于通用层，移入 HelloUtauWidgets，接口不含编辑器的类型，HelloUtauWidgets 因此私有链接 QActionKit）。登记所在的列表 `ActionContributionRegistry` 是公开的，由 `Editor` 持有，见「注册接口」。动作以窗口为父对象，贡献移除时由编辑器从 context 移除并删除。插进某种窗口菜单的条目若没有该窗口的动作，context 显示一个不做任何事的占位项。
- **`ActionRegistration`**：登记对象，登记到某个 `Editor` 的 `ActionContributionRegistry`。编辑器随登记加入清单与各窗口的动作，随注销移除，然后刷新各窗口的菜单、文字、快捷键与图标。用户的快捷键与菜单改动在编辑器构造时读入，QActionKit 保存这些改动并在每次加入清单后重新应用，因此晚于读入登记的清单同样得到用户的设置。窗口创建时加入已有贡献的动作。命令面板取 registry 与 context 的交集，插件的命令自动出现在其中。
- **编辑器自己的清单也经此登记**（作者 2026-09-30，方案 1 加 3；2026-10-01 改为每种窗口一份）：`ProjectActions.xml` 与 `VoiceBankActions.xml` 各含一种窗口的全部命令、菜单栏与工具栏，布局写全，不用插入；两种窗口共有的命令在两份中各声明一次。两份由 `BuiltinActions` 分别登记到两种窗口的 registry；它是编辑器的子对象，Core 插件为其编辑器创建一个，不经 Core 构造 `Editor` 的测试同样为每个编辑器创建一个。处理函数仍在窗口中，窗口自己创建这些动作。
- **最终目标**（作者 2026-09-30）：清单与处理函数都由 Core 插件提供，窗口只提供能力。这需要窗口公开相应的操作，届时另行设计。
- **现状的两条路径**：内置命令的处理函数在窗口私有的实现中，由窗口在 `initActions()` 中自己创建动作，`BuiltinActions` 的贡献不创建动作；插件的动作由其贡献的 `addActions()` 创建并连接。插件能连接自己的处理函数，但处理函数只能使用窗口的公开接口，目前很少。内置命令移入贡献之后只剩一条路径。
- **不在菜单中的命令**：动作的快捷键经菜单栏生效，不在任何菜单中而有快捷键的命令（如 ClassicPluginHost 的 Classic Plugins at Pointer），由贡献方以 `window->addAction()` 将动作挂到窗口上（作者 2026-09-30 定）。未来可选：由编辑器统一将快捷键范围为 `WindowShortcut` 的动作挂到窗口上。重复挂载不会使快捷键触发两次，但须跳过另设范围的动作（如音源窗口波形区的 `WidgetWithChildrenShortcut`），否则其快捷键扩展到整个窗口。
- **编辑器的组件化**（作者 2026-09-30 定的方向）：`Editor` 与窗口逐步拆成组件，最终只保留较底层、偏元的功能（窗口、文档、动作与设置的基础设施），高级功能由插件组合而成。每移出一块功能，先让窗口公开它所需的能力，再把命令与界面移入插件。
- **静态字符串**：AEC 生成的字符串是 `QStringLiteral`，位于清单所在库的静态存储中，由条目取得的 `QString` 与之共享数据。目前所有库都在 Core 销毁 `Editor` 之后才卸载，这些副本随 `Editor` 一起销毁，因此没有问题。将来支持在运行中停用单个插件时，须让 AEC 生成自有内存的字符串。
- **测试**：`test_ActionContribution`（登记前后打开的窗口、注销、先于或晚于 `Editor` 的登记）；TestAction 插件（`tests/auto/plugins/TestAction`，依赖 Core，向工程窗口的 Tools 菜单加入 Hello）由 `test_TestActionPlugin` 与 Core 一同载入。

### 异步

stdcorelib.plugin 的生命周期是同步的，不依赖事件循环。HelloUtau **现在不实现任何异步机制**，将来需要时以组合完成，不修改该库，也不给插件增加钩子：

- 插件自己的异步工作由插件以 Qt 完成（`QTimer`、`QThread` 等），宿主不参与。插件异步准备好后才登记的内容，由注册表的增删通知接住。
- 退出时须等待插件工作结束的情形（异步关闭），届时以一个「退出前待完成的工作」的登记接口实现：插件为这类工作登记一项；`Editor` 退出时关闭窗口，通知各登记项结束，在正常的事件循环中等待（有超时，结束通知一律排队回到主线程），全部结束后才调用同步的 `shutdownPlugins()`。不使用嵌套事件循环。出现这类插件之前不做，插件在 `aboutToShutdown()` 中同步结束自己的工作。
- 延迟初始化的钩子不加：没有实现的钩子只会令人困惑（作者 2026-09-30）。

### 对 stdcorelib.plugin 的补充

- `PluginSystem::loadOrder()`（作者 2026-09-30 同意可加，已实现，见实施步骤 7）：返回 `loadPlugins()` 实际使用的依赖优先顺序。停用、未被载入判定选中、在载入前已无效的插件不在其中；载入或初始化失败的插件仍在其中并带有错误。解析依赖之前为空，关闭后不变。HelloUtau 目前不依赖它，它服务于插件管理页的列表与诊断，以及该库将来的分批调度。

## 实施步骤

1. ~~**依赖**~~：stdcorelib.plugin 与 stdcorelib 一样单独构建安装（动态库），`third-party/Dependencies.cmake` 以 `-Dstdcorelib-plugin_DIR=` 引入（2026-10-08 起可出现在公开头文件中），Windows 上其 DLL 复制到运行输出目录。README、CLAUDE.md 与 Status.md 已补充。
2. ~~**note.md**~~：已按作者的决定改写插件一节（一种原生插件、五个扩展点、UTAU 插件由一个原生插件支持），CLAUDE.md、Roadmap.md、Status.md、Interchange.md、FrequencyTables.md 的相应说法一并更新。
3. ~~**加载器与 Core 插件**~~：`AppLoader`、Core 插件、`helloutau_add_native_plugin()`，程序只剩加载器。`test_AppLoader` 以测试插件覆盖参数、Core 插件的必需与三种失败、生命周期，并载入真正的 Core 插件打开窗口。
   - ~~**插件作为库与动作的扩展**~~：插件导出目标、安装头文件；`ActionContribution` / `ActionRegistration`；编辑器的清单拆为两份，经 `BuiltinActions` 由 Core 登记；TestAction 测试插件。见「编辑界面扩展：动作与命令」。
4. ~~**试点：频率表格式与 FrequencyEditor 插件**~~：
   - `FrequencyFormatRegistration`（HelloKitVoiceBank）：登记对象，持有一种格式，形状与 `ActionRegistration` 相同（见「注册接口」）。
   - `FrequencyFormatRegistry` 成为 `QObject`：内容来自进程级列表，按登记顺序；同一 ID 的多个格式只取先登记的，它注销后由下一个接替，与 qactionkit 对重复条目的处理一致；变化时发出 `formatsChanged()`。不再有 `add()` 与 `addBuiltinFormats()`。音源窗口的「F0」下拉框随之重建：所选格式仍在则保留，否则改选重采样器对应的格式。
   - `BuiltinFrequencyFormats` 登记 frq、dio、mrq，FrequencyEditor 插件（ID `org.helloutau.frequencyeditor`，目录 `FrequencyEditor`，依赖 Core，作者定名）持有它；读取这些文件的代码留在 hellokit。将来的频率表编辑界面也由它负责。需要这些格式的测试自己持有一个。
   - 删除 `FrequencyFormatPlugin`。
   - 多个格式都匹配重采样器时，默认选**后登记的**，即后载入的插件的格式（作者 2026-09-30 决定）：想接管内置处理的插件依赖 FrequencyEditor，必在其后载入。
   - `AppLoader::errors()` 列出核心插件以外载入失败的插件，`test_AppLoader` 据此检查随应用提供的插件全部载入。起因：`helloutau_add_native_plugin()` 的 `DEPENDENCIES` 原为多值参数，吞掉了其后交给 `helloutau_add_plugin()` 的参数，FrequencyEditor 的 `plugin.json` 因而带有虚假的依赖而载入失败，只写入日志，测试没有发现。`DEPENDENCIES` 现为单值参数，多个依赖以分号分隔。
5. **设置**：~~用户的启用设置文件~~（`plugins.json`，见上文「设置」），~~设置对话框的「Plugins」页~~（见上文），随安装提供的全局设置（未做）。
6. ~~**格式转换驱动**~~：`InterchangeRegistration`（HelloKitInterchange，一个导入或导出驱动）、进程级列表 `InterchangeRegistrations_p.h`、`InterchangeRegistry` 成为监听列表的 `QObject`（`driversChanged()`），`BuiltinInterchangeDrivers` 登记 MIDI 的读与写，由新的 Interchange 插件（ID `org.helloutau.interchange`，依赖 Core，作者 2026-09-30 定）持有；删除 `InterchangePlugin`。规则见 [`Interchange.md`](Interchange.md)「注册表」。
7. **ClassicPluginHost 插件**（计划见 [`ClassicPluginHost.md`](ClassicPluginHost.md)）：随 HelloUtau 提供的原生插件，把 UTAU 插件作为命令加入「工具 → Classic Plugins」菜单，运行后把结果作为一个撤销步骤应用到选区。选区编辑的注册接口暂不建。验收同 Roadmap 第五阶段：若干社区常用的原版插件能够正常执行并写回结果。
8. **其余扩展点**：编辑界面扩展、音源批量操作，随各自功能的实现加入。
9. ~~**stdcorelib.plugin 的 `loadOrder()`**~~：已实现（该仓库 `e1f7ad6`），测试覆盖依赖链与可选依赖、同层按发现顺序、停用与未选中与无效插件的排除、失败插件的保留、载入中的重入查询。「Plugins」页须列出停用的插件，而 `loadOrder()` 不含停用的插件，因此该页按发现顺序列出（`plugins()`），不使用 `loadOrder()`。

## 作者的决定（2026-09-30）

1. note.md 的插件定义改为一种原生插件与五个扩展点。原版 UTAU 插件的功能本身实现为一个原生插件。
2. 登记对象在 `initialize()` 中创建。
3. 插件目录用目录布局（`Bundle`）。
4. 接受 HelloUtau 作为 stdcorelib.plugin 的首个真实使用者之一，其 API 可能随之调整。
5. 目录布局的元数据文件名沿用 `plugin.json`。
6. IID 为 `org.OpenVPI.HelloUtau.Plugin`，常量放在 HelloUtauEditor。
7. 程序只是加载器（`AppLoader`，不用 `Loader` 这样的名称），原来程序入口的工作由 Core 插件负责；目录为 `bin` 与 `lib/plugins/helloutau/<插件>`；程序文件名为小写的 `helloutau`，Windows 资源中的名称显式为 `HelloUtau`。
8. 频率表插件名为 FrequencyEditor。多个格式匹配重采样器时选后载入的。
9. 插件可以注册自己的动作（命令），属于编辑界面扩展。插件也是库：导出目标、安装头文件，插件中库一级的内容在 `tests/auto/plugins` 中测试，不测试菜单布局。
10. 编辑器的清单由 Core 注册：现在按方案 1 加 3（清单拆成应用层与窗口层两份，都经 Core 登记，处理函数仍在窗口中），最终目标是方案 2（清单与处理函数都在 Core 中）。
