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

**结论：采用 stdcorelib.plugin。** 它与「只提供注册接口」的设想一致，许可证与分层都与本仓库相容，符合 AGENTS.md「基础设施优先使用 stdcorelib」。代价是 API 可能随迁移反馈调整，HelloUtau 将是它的首个真实使用者之一；与 stdutau、substate 相同，发现不足时直接修改该库。

### stdcorelib.plugin 提供的部分

- `stdc::plugin`：插件是某个 IID 接口的实现，以 `STDC_EXPORT_PLUGIN` 导出，IID 与元数据嵌在库中；`PluginFactory` 按目录发现。
- `stdc::pluginsystem::PluginSystem`：只接收一个 IID；元数据保留 `id`、`displayName`、`version`、`compatVersion`、`enabledByDefault`、`dependencies`，其余字段归宿主；依赖解析、按依赖顺序载入并调用 `initialize()`、逆序调用 `pluginsInitialized()` 与 `aboutToShutdown()` 并卸载；全局与用户两份启用设置；宿主的载入判定（`setPluginLoadPredicate()`）；Flat、Bundle 与自定义三种目录布局。
- 插件实现 `stdc::pluginsystem::IPlugin` 的三个钩子：`initialize()`、`pluginsInitialized()`、`aboutToShutdown()`。
- stdcorelib 另有 `stdc::DynamicRegistry`：运行时增删、线程安全、每个进程一份（stdcorelib 是动态库时），可监听条目的增删。

## 设计

### 一种原生插件，若干扩展点

- 原生插件只有一种：实现 `stdc::pluginsystem::IPlugin`，IID 为 HelloUtau 的一个固定值（暂定 `org.qsynthesis.HelloUtau.Plugin`），以 `STDC_EXPORT_PLUGIN` 导出。**本仓库不定义任何插件类。**
- 插件在 `initialize()` 中经本仓库的注册接口登记它提供的东西。note.md 中原来的五种 C++ 插件改为五个**扩展点**：频率表格式、格式转换驱动（读写器）、选区编辑、编辑界面扩展、音源批量操作。
- **原版 UTAU 插件的支持本身也是一个原生插件**（作者 2026-09-30 决定）：随 HelloUtau 提供，放在程序旁的 `plugins` 中。它在 `initialize()` 中发现 UTAU 插件文件夹（`plugin.txt`，有 `plugin.json` 时以其为准，编码规则见 note.md），把每个 UTAU 插件登记为一项选区编辑，运行时写出 `temp.ust`、启动可执行文件、读回结果。停用它即不再提供 UTAU 插件，应用本身不含 UTAU 插件的代码。它依赖的 `temp.ust` 读写与编码处理在 hellokit 中，插件链接 hellokit 使用。
- 删除 `FrequencyFormatPlugin` 与 `InterchangePlugin` 两个接口。

### 注册接口

- 每个扩展点在其所属的 hellokit 子库中有一张**进程级的工厂表**，内部以 `stdc::DynamicRegistry` 实现。stdcorelib 仍只是私有依赖：公开头文件中只有本仓库的类型。
- 公开的是一个**登记对象**：构造时把「标识、名称、工厂」登记进表，析构时注销。插件把登记对象作为自己的成员，在 `initialize()` 中创建，随插件实例析构而注销。
- 内置的格式与驱动经同一途径登记，与插件不分主次，保持 `InterchangeRegistry` 与 `FrequencyFormatRegistry` 已有的原则。
- 应用与测试持有的注册表（`FrequencyFormatRegistry` 等）**仍然不是全局单例**：它们从工厂表取得实例。测试可以只放入自己需要的内容，不受进程级状态影响。
- 注册表订阅工厂表的增删通知（`DynamicRegistry` 的监听），因此插件在运行中才登记或注销也能反映到界面，例如音源窗口的「F0」下拉框。
- 登记对象在 `initialize()` 中创建（作者 2026-09-30 决定）：生命周期明确，可带运行时参数，例如 UTAU 插件支持插件按发现结果登记的各项。未采用的做法是以静态对象在插件载入时自动登记：名称须是字面量、登记不加锁，且公开头文件中会出现 stdcorelib 的类型。

### 载入、设置与关闭

- `Editor` 持有一个 `PluginSystem`，在创建窗口前完成载入。
- 目录与布局：**目录布局**（stdcorelib.plugin 的 `Bundle`，作者 2026-09-30 决定）。每个插件占搜索目录下的一个子目录，其中有插件的库与一个元数据 JSON：根字段 `name` 给出库的平台无关名（`name` 为 `vs4ufrq` 时可对应 `vs4ufrq.dll`、`libvs4ufrq.so`、`libvs4ufrq.dylib`），其余为 `id`、`displayName`、`version`、`dependencies` 与宿主字段。IID 仍以 `stdc_add_plugin_metadata()` 嵌在库中。插件的其他文件（翻译、图标、数据）放在同一子目录中。搜索目录为程序旁的 `plugins` 与用户配置目录下的一个目录，都与 UTAU 插件的目录分开。
- 元数据文件名：沿用该库默认的 `plugin.json`，以 `PluginSystem(iid, PluginSystem::Bundle)` 直接构造（作者 2026-09-30 决定）。它与 UTAU 插件文件夹的 `plugin.json` 同名，但两种目录不会互相搜索，库中的 IID 也能区分原生插件，不会误读。
- 兼容性：C++ 插件须与宿主以同一编译器、同一 Qt 与 hellokit 版本构建。元数据加一个宿主字段（如 `helloutau` 的版本范围），由载入判定检查；判定也用于平台限制（如只在 Windows 可用的 vs4ufrq 格式插件）。
- 设置：用户的启用设置存为用户配置目录中的一个 JSON 文件（`PluginSettings`），全局设置随安装提供。设置对话框增加「Plugins」页：列出插件、勾选启用、显示错误与依赖。
- **关闭顺序**：插件登记的工厂与由它造出的对象，代码都在插件的库中，必须在卸载前销毁。`Editor` 退出时先关闭窗口、释放各注册表中的实例，再调用 `shutdownPlugins()`。

### 异步

stdcorelib.plugin 的生命周期是同步的，不依赖事件循环。HelloUtau **现在不实现任何异步机制**，将来需要时以组合完成，不修改该库，也不给插件增加钩子：

- 插件自己的异步工作由插件以 Qt 完成（`QTimer`、`QThread` 等），宿主不参与。插件异步准备好后才登记的内容，由注册表的增删通知接住。
- 退出时须等待插件工作结束的情形（异步关闭），届时以一个「退出前待完成的工作」的登记接口实现：插件为这类工作登记一项；`Editor` 退出时关闭窗口，通知各登记项结束，在正常的事件循环中等待（有超时，结束通知一律排队回到主线程），全部结束后才调用同步的 `shutdownPlugins()`。不使用嵌套事件循环。出现这类插件之前不做，插件在 `aboutToShutdown()` 中同步结束自己的工作。
- 延迟初始化的钩子不加：没有实现的钩子只会令人困惑（作者 2026-09-30）。

### 对 stdcorelib.plugin 的补充

- `PluginSystem::loadOrder()`（作者 2026-09-30 同意可加，已实现，见实施步骤 7）：返回 `loadPlugins()` 实际使用的依赖优先顺序。停用、未被载入判定选中、在载入前已无效的插件不在其中；载入或初始化失败的插件仍在其中并带有错误。解析依赖之前为空，关闭后不变。HelloUtau 目前不依赖它，它服务于插件管理页的列表与诊断，以及该库将来的分批调度。

## 实施步骤

1. **依赖**：stdcorelib.plugin 与 stdcorelib 一样单独构建安装，`third-party/Dependencies.cmake` 以 `-Dstdcorelib-plugin_DIR=` 引入，作为私有依赖。AGENTS.md 与 Status.md 相应补充。
2. ~~**note.md**~~：已按作者的决定改写插件一节（一种原生插件、五个扩展点、UTAU 插件由一个原生插件支持），AGENTS.md、Roadmap.md、Status.md、Interchange.md、FrequencyTables.md 的相应说法一并更新。
3. **试点：频率表格式**。工厂表、登记对象、`FrequencyFormatRegistry` 改为从工厂表取得并订阅增删；内置的 frq、dio、mrq 经登记对象登记；删除 `FrequencyFormatPlugin`。测试中构建一个真实的插件模块（`MODULE` 目标，`stdc_add_plugin_metadata()`），复制到临时目录的一个子目录并写出其元数据，由 `PluginSystem` 以目录布局载入、登记一个格式、关闭后注销。
4. **应用**：`Editor` 持有 `PluginSystem`，载入与关闭顺序，用户设置文件，设置对话框的「Plugins」页。
5. **格式转换驱动**：同样改为注册接口，删除 `InterchangePlugin`。
6. **选区编辑与 UTAU 插件支持插件**：选区编辑的注册接口；随 HelloUtau 提供的原生插件，把 UTAU 插件登记为选区编辑；应用的「插件」菜单列出已登记的选区编辑。验收同 Roadmap 第五阶段：若干社区常用的原版插件能够正常执行并写回结果。
7. **其余扩展点**：编辑界面扩展、音源批量操作，随各自功能的实现加入。
8. **stdcorelib.plugin 的 `loadOrder()`**：已实现（该仓库单独提交），测试覆盖依赖链与可选依赖、同层按发现顺序、停用与未选中与无效插件的排除、失败插件的保留、载入中的重入查询。「Plugins」页在第 4 步使用它。

## 作者的决定（2026-09-30）

1. note.md 的插件定义改为一种原生插件与五个扩展点。原版 UTAU 插件的功能本身实现为一个原生插件。
2. 登记对象在 `initialize()` 中创建。
3. 插件目录用目录布局（`Bundle`）。
4. 接受 HelloUtau 作为 stdcorelib.plugin 的首个真实使用者之一，其 API 可能随之调整。
5. 目录布局的元数据文件名沿用 `plugin.json`。
