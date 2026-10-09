# 导入与导出界面

本文档规定格式转换在应用一侧的界面，写于 2026-09-30。驱动、选择器与注册表的规定见 [`Interchange.md`](Interchange.md)。本文对应 [`Roadmap.md`](Roadmap.md) 第三阶段遗留的轨道选择与插入位置，以及 Interchange.md「自定义选择步骤」的第一个实例（MIDI 的编码页）。实施状态见文末「实施步骤」。

## 范围

- 包含：「文件」菜单中其他格式的导入与导出命令；导入向导（文件与格式、选项或自定义页、条目、插入位置、结果）；MIDI 的编码页；导出向导；诊断的显示。
- 不包含：新的格式驱动（VSQ、SynthV，`.ustx` 不内置，见 [`Roadmap.md`](Roadmap.md)）、多轨导入、`.uar` 音源安装（作者 2026-09-30 定为暂缓）。

## UST 的处理

- `.usth` 与 `.ust` 属于 `HelloKitDocument`，不是格式转换驱动，不登记到 `InterchangeDrivers`（Interchange.md「职责边界」）。
- UST 的打开仍为「文件 → 打开」。该流程有专用的编码对话框（`UstCharsetDialog`），文档记录来源文件，保存时以同名的 `.usth` 为默认文件名。
- UST 的导出仍为 Editor 的「文件 → 导出 → UST…」。导出时写入本机设置中的合成工具，并按目标文件写入 `CacheDir`。二者取自应用设置，无法表示为驱动的选项。
- Interchange 插件的导出命令插入同一「导出」子菜单，位于 UST 之后。停用 Interchange 插件只移除其他格式，不影响 UST。
- 编辑器组件化（[`Plugins.md`](Plugins.md)「最终目标」）时，UST 的导出随其余内置命令移入 Core，不属于本文范围。

## 作者的决定（2026-09-30）

1. 菜单只有「文件 → 导入…」与「文件 → 导出 → 其他格式…」两项。格式在向导中选择，不按驱动逐项列出。
2. 导入与导出均为向导（`QWizard`），插入位置是导入向导的一页。
3. 没有选区时插入到工程末尾。
4. 导入只并入当前工程，不提供「新建工程」一项。导入为新工程的操作为先新建工程再导入。
5. 诊断显示在向导的最后一页，不另弹对话框。

## 结构

全部位于 Interchange 插件中（作者 2026-09-30 决定格式转换的界面归属该插件）。

- 插件持有一个 `InterchangeService`，其中有驱动表 `InterchangeDrivers` 与步骤页的注册表 `InterchangeStepRegistry`。向导中的格式列表与文件对话框的过滤器由驱动表生成。其他插件经 `InterchangeService::instance()` 登记驱动与步骤页（见 Plugins.md「注册接口」）。
- 插件的公开头文件（`helloutau/plugins/Interchange/`）提供库一级的类，在 `tests/auto/plugins/Interchange/` 中测试：
  - `ImportMerge`：应用层的插入逻辑（Interchange.md「应用层的导入流程」），无界面。以 `ProjectRef` / `NoteRef` 的公开接口在一个事务中修改工程，与 ClassicPluginHost 应用结果的方式相同。库中不需要新增接口。
  - `PresetSelector`：返回向导预先收集的请求的选择器，见下文「向导与选择器」。
  - `SourcePreview`：以指定编码解码探查结果中的文本，供编码页预览，并给出默认选中的编码。
- `InterchangeStepPage` 与 `InterchangeStepRegistry` 原定放在 HelloUtauEditor 或 Widgets（Interchange.md「自定义选择步骤」），改为放在插件的公开头文件中，登记形状同 Plugins.md「注册接口」：`InterchangeStepRegistry::AddFactory(service.stepPages(), id, 描述, 工厂)`。提供新格式的插件依赖 Interchange 插件并以此登记自定义页。MIDI 的编码页由 Interchange 插件登记。

### 向导与选择器

`InterchangeReader::read()` 是非虚函数，依次执行探查、选择器询问与转换。向导则须在用户逐页选择时显示探查结果。二者的衔接方式如下：

- 选定文件后，向导调用公开的 `inspect()`，以其结果显示条目与预览，并逐页收集选择。
- 用户确认后，向导以 `PresetSelector` 调用 `read()`，`selectImport()` 返回预先收集的 `ImportRequest`。探查因此执行两次，额外开销为再读一次文件。其收益是导入仍只经由 `read()` 这一条路径，与命令行和测试一致。
- 两次探查之间文件若被修改，条目可能不再对应。`PresetSelector` 核对条目数与条目索引，不一致时记录 `Error` 并返回 `std::nullopt`，结果页显示该原因。

导出同理：`selectExport()` 返回向导收集的 `ExportRequest`。

## 导入向导

入口为「文件 → 导入…」，仅在工程窗口中提供。各页如下：

1. **文件**：路径输入框与「浏览…」按钮。格式下拉框为「按扩展名」与每个导入驱动各一项。「浏览…」的过滤器为「所有支持的格式」与每个驱动各一项，选择某一驱动的过滤器即同时选择该格式。按扩展名找不到驱动时不能继续。离开本页时调用 `inspect()`，失败时在本页显示诊断，不能继续。
2. **选项**：驱动有已登记的自定义页时显示该页，否则显示由 `optionSchema()` 生成的表单，二者择一。自定义页未登记时显示表单并记录一条诊断。驱动没有选项时跳过本页。
3. **条目**：表格，列为名称、音符数、音域。驱动声明了键为 `encoding` 的选项时，名称以上一页选择的编码解码，否则以 UTF-8 解码。解码失败时显示「无法解码」，不显示乱码。表格为单选，选择另一条目即替换原选择（Interchange.md「用户决策」）。文件只有一个条目时默认选中。
4. **插入位置**：「插入到选区之后」「插入到选区之前」「替换选区」。没有选区时只有「插入到末尾」。另有「保留 tempo」「保留开头的休止符」两个选项，默认保留 tempo、去掉开头的休止符。当前工程没有音符时两项均默认保留，以保持导入音符的小节位置。本页为提交页（`QWizardPage::setCommitPage`），提交后不能返回。
5. **结果**：进入本页时执行导入并插入工程，列出诊断。有 `Error` 时工程不变，本页说明未导入；否则说明插入的音符数，并选中插入的音符。`Note` 级诊断（如「以默认值代替用户选择」）同样列出，以信息图标区分。

### MIDI 的编码页

- 左侧为编码列表，右侧预览轨名、歌词与标记，切换编码时全部重新解码。候选为驱动在 `encoding` 选项中声明的值，无法解码文件文本的编码以禁用文字的颜色显示，同 `UstCharsetDialog`。
- 默认选中项：先尝试 UTF-8，出现非法字节序列时改用系统编码，二者都不适用时取 `TextCodec::ranked()` 的首位（Interchange.md「默认编码可以推测」）。
- 编码列表与预览之下是驱动其余选项的生成表单（无歌词音符的默认歌词），自定义页负责驱动的全部选项。
- 所选编码经 `ImportRequest::driverOptions["encoding"]` 传给驱动。

### 插入规则

- 导入工程的工程级设置与轨道设置（名称、轨名、音源目录、flags）一律丢弃，并记录一条 `Note` 级诊断（Interchange.md）。
- 与当前工程音高模式不同的音高数据（Mode1 的 `pitchBend` 或 Mode2 的 `portamento`）被删除，并记录一条 `Warning` 级诊断。
- 保留 tempo 时，若导入工程的 tempo 与插入位置的 tempo 不同，将其写入第一个插入音符的 `tempo`。导入文件中途的 tempo 本已记录在音符上。不保留 tempo 时，清除所有插入音符的 `tempo`。
- 插入音符之后的第一个原有音符若没有显式的 `tempo`，且插入改变了其前生效的 tempo，则为该音符写入插入前生效的 tempo，使其后部分的速度不变。「替换选区」删除的音符若带有 `tempo`，同样按此规则处理。
- 去掉开头的休止符指删除插入音符序列开头连续的休止符。删除后没有音符时，工程不变，记录一条 `Warning` 级诊断。
- 全部修改构成一个撤销步骤。

## 导出向导

入口为「文件 → 导出 → 其他格式…」。各页如下：

1. **文件**：格式下拉框为每个导出驱动各一项。路径默认取文档的来源文件或 `.usth` 文件，扩展名随所选格式调整。目标文件已存在时，离开本页前确认是否替换，同 UST 导出。
2. **选项**：由 `optionSchema()` 生成的表单。格式没有选项时显示「此格式没有选项」。本页为提交页。
3. **结果**：进入本页时写出文件，列出诊断（MIDI 导出报告无法表示的参数，见 `MidiWriter` 的说明）。

## 实施步骤

1. ~~插件持有注册表；「导入…」与「导出 → 其他格式…」两个命令；`ImportMerge` 及其测试。~~
2. ~~向导：文件、由 `optionSchema()` 生成的表单、条目、插入位置、结果各页；`PresetSelector` 及其测试。~~ 待作者试用界面。
3. ~~自定义页：`InterchangeStepPage` / `InterchangeStepRegistration` / `InterchangeStepRegistry`，MIDI 的编码页（`Internal/MidiEncodingPage`），编码的预览与默认选择（`SourcePreview`）及其测试。~~ 待作者试用界面。
4. ~~Interchange.md、Plugins.md、Status.md 的相应更新。~~
