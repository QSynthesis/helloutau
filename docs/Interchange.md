# 格式转换模块 `HelloKitInterchange`

本文档规定 `hellokit/lib/Interchange/` 的职责边界与接口结构。产出目标为 `HelloKitInterchange`，include 路径为 `<hellokit/Interchange/...>`，命名空间为 `hello::kit`。

该模块在 [`Roadmap.md`](Roadmap.md) 中对应第六阶段的「MIDI / VSQ / SynthV 导入」，但 **MIDI 导入的优先级被提至最高**，见文末。

## 职责边界

**`HelloKitDocument` 负责 HelloUtau 自身的格式，`HelloKitInterchange` 负责外部格式。**

| | 归属 |
|---|---|
| `.usth`、`.ust` | `HelloKitDocument` |
| MIDI 导入与导出、VSQ / VSQX 导入、SynthV `.svp` 导入、OpenUTAU `.ustx` 导入 | `HelloKitInterchange` |
| `presamp`、`frq` | `HelloKitVoiceBank` |

`presamp` 和 `frq` 在 Roadmap 中与 MIDI 列于同一行，那是按「兼容周边」分类，而非按模块分类。这两者描述的是音源而非工程。

**依赖方向为 `HelloKitInterchange` → `HelloKitDocument`，不得反向。** 转换的中间表示即 Document 的 `Project`，Interchange 只负责将外部格式翻译为该表示。

## 三条硬性约束

### 一、忠实翻译，不做取舍

**Interchange 只翻译文件中已有的内容，不判断用户是否需要。** 取舍属于应用层的导入流程，不属于本模块。

两个具体结果：

- **tempo 原样翻译。** 首个 tempo 存入 `settings.tempo`，中途的 tempo 存入对应音符的 `note.tempo`。应用层若不需要，由其自行清除。
- **前导休止符照常补充。** 第一个音符之前的静默在源文件中真实存在，翻译应当将其保留。插入现有工程时是否裁掉由应用层决定，因为只有应用层知道插入位置。

这一约束不是风格偏好。它决定了哪些内容**不应**出现在 `ImportRequest` 中：任何形如「是否保留 X」的选项都表明取舍被放在了错误的层。

### 二、有损转换是常态，因此诊断必须是返回值的一部分

MIDI 除歌词外不包含任何 UTAU 参数，VSQ 的参数与 UTAU 并非一一对应，源文件中的多声部进入 UST 时必须简化。**转换丢失数据是必然的，并非缺陷，但静默丢失是缺陷。**

因此每次转换都产出一份诊断列表，能够定位到具体音符，由应用层展示。仅写入日志不足以向用户说明。

`Diagnostic` 类型**不属于本模块**。音源扫描、合成、工程读取都需要报告同类信息，因此它应当是 `HelloKitSupport` 中的公共类型，Interchange 只是它的第一个使用者。

### 三、`hellokit` 不链接 QtWidgets

接口中不得出现对话框。需要用户决定的事项通过 `InterchangeSelector` 回调传出，由链接 QtWidgets 的一侧实现。

## 接口

### 探查与条目

```cpp
/// 文件中一个可导入的单元：MIDI 的一个 track、VSQ 的一个 part、ustx 的一个轨道。
struct InterchangeEntry {
    int index;
    int noteCount;
    std::optional<int> lowestNote;    // 音域，供界面显示
    std::optional<int> highestNote;

    // 编码尚未确定，因此此处为未解码的字节。见下文。
    QByteArray rawName;
    QList<QByteArray> rawLyrics;
};

/// 探查结果，在任何转换之前即可获得。
struct InterchangeSource {
    QString formatId;
    QList<InterchangeEntry> entries;
    QList<QByteArray> rawLabels;
};
```

`inspect()` 是公开的，因为「只查看文件内容」是一项独立需求（文件对话框预览），不应被迫执行完整的导入流程。

### 未解码字节的理由

`rawName` 和 `rawLyrics` 的类型是 `QByteArray` 而非 `QString`，这是**有意为之，且是本模块中唯一的例外**。

编码由用户在该界面上选择，选择之前无法确定如何解码。要让用户对照预览选择编码，界面必须取得原始字节，并按当前选中的编码即时解码。若在 `inspect()` 中预先解码后再交出，就等于在用户作答之前替用户做出了选择。

这是 [`AGENTS.md`](../AGENTS.md) 中「原始字节不得离开 I/O 边界层」这一规则的**有限例外**，其边界如下：

- 只有 `InterchangeSource` 这一个结构可以携带原始字节。
- **`read()` 返回的 `Project` 中不得包含任何原始字节**，此时编码已经确定，全部内容均为 UTF-8。
- 即原始字节只存在于「编码尚未选定」这一阶段，该阶段结束后即不再存在。

**解码失败必须明确显示，不得显示为乱码。** 无法解码的条目在预览中明确标注「解码失败」，使用户能立即看出所选编码有误。

### 驱动

导入和导出是两个纯虚类，**不合并为一个带能力掩码的类**。大部分格式只支持导入，若使用单一接口加掩码，「不支持的方向」只能返回 false 并依赖文档约束；拆分后则由类型系统保证。同时实现两者的驱动分别注册两次。

```cpp
class HELLOKIT_INTERCHANGE_EXPORT InterchangeReader {
public:
    virtual ~InterchangeReader();

    virtual QString id() const = 0;             // "midi"
    virtual QString name() const = 0;           // "Standard MIDI File"
    virtual QStringList suffixes() const = 0;   // {"mid", "midi"}

    /// 该驱动接受的选项。界面据此生成控件，AutomaticSelector 据此取默认值。
    virtual QList<InterchangeOption> optionSchema() const { return {}; }

    /// 需要自定义选择步骤时返回其 ID，空表示使用由 optionSchema() 生成的通用表单。
    virtual QString customStepId() const { return {}; }

    /// 探查，不转换。
    virtual std::optional<InterchangeSource> inspect(const std::filesystem::path &path,
                                                     DiagnosticList &diagnostics) = 0;

    /// 完整流程：探查、询问 selector、转换。非虚函数，驱动不实现。
    ImportResult read(const std::filesystem::path &path, InterchangeSelector *selector,
                      const ImportLimits &limits = {});

protected:
    /// 所有设置确定后，按 request 转换。由 read() 调用。
    virtual std::optional<Project> convert(const std::filesystem::path &path,
                                           const InterchangeSource &source,
                                           const ImportRequest &request,
                                           DiagnosticList &diagnostics) = 0;
};
```

方法名为 `read` / `write` 而非 `import` / `export`，因为 **`export` 是 C++ 关键字**。

**`read()` 是非虚函数，驱动只实现 `inspect()` 和 `convert()`。** 探查、询问 selector、转换这三步的编排位于基类中，因为若由调用方自行组合，就多出一处可能弄错顺序的地方，而命令行、测试和编辑器必须以完全相同的方式导入。

### 选项的两个层次

```cpp
struct InterchangeOption {
    QString key;                  // "encoding"
    QString name;                 // 显示名称
    enum Type { Bool, Int, Enum, Text } type;
    QVariant defaultValue;
    QStringList choices;          // 用于 Enum
};

struct ImportRequest {
    QList<int> entries;           // 选中的条目，所有格式通用
    QVariantMap driverOptions;    // 驱动自行声明的选项，键名见 optionSchema()
};
```

**条目选择保持强类型，不纳入 schema。** 每种格式都有条目选择，且界面需要显示的信息远多于一个下拉框：轨名、音符数、音域。放入 `QVariantMap` 会丢失这些信息的结构。

**其余选项一律由驱动自行声明。** 文本编码是典型例子：MIDI 是早期格式，歌词和轨名均无编码声明，只能询问用户；而较新的 `.ustx`、`.svp`、`.vsqx` 均使用 Unicode，不存在这一问题。将 `encoding` 放入公共结构，等于让三种格式承担一个只有第四种格式需要的字段。

**但编码本身不经由该 schema，它是自定义步骤的第一个使用者**，理由见下一节。schema 适用于「一个控件对应一项设置」的选项，例如「丢弃短于多少 tick 的音符」。

### 用户决策

```cpp
/// 文件未指定、只能由用户决定的事项。由链接 QtWidgets 的一侧实现。
class HELLOKIT_INTERCHANGE_EXPORT InterchangeSelector {
public:
    virtual ~InterchangeSelector();

    virtual std::optional<ImportRequest> selectImport(const InterchangeReader &reader,
                                                      const InterchangeSource &source,
                                                      const ImportLimits &limits,
                                                      DiagnosticList &diagnostics) = 0;
    virtual std::optional<ExportRequest> selectExport(const InterchangeWriter &writer,
                                                      const Project &project,
                                                      DiagnosticList &diagnostics) = 0;
};

/// 导入目标的容量，由调用方提供。
struct ImportLimits {
    int minEntries = 1;
    int maxEntries = 1;    // 工程目前为单轨，因此为 1
};
```

一次调用完成全部询问，不拆分为「选择轨道」「选择编码」两个回调，否则界面上会连续弹出两个对话框。驱动通过 `reader` 参数提供 `optionSchema()`。

**可选条目数由调用方指定，而非驱动。** 驱动只知道文件中有多少条目，不知道导入目标能容纳多少：工程目前为单轨，因此 `maxEntries` 为 1；将来开放多轨时修改的是调用方，驱动无需任何改动。界面在**已选满后再次选择**时，应当替换最早选中的条目，而不是拒绝用户的点击。

**`nullptr` 不得导致阻塞或崩溃。** kit 内置 `AutomaticSelector`：按 `optionSchema()` 取每一项的默认值，选中全部条目，并且**每代替用户做一次决定就记录一条 `Note` 级诊断**。命令行工具、测试以及将来的批量转换都使用它，传入 `nullptr` 即使用它。这样测试永远不会停在一个得不到回应的对话框上。

### 取消与失败的区分

用户关闭对话框与文件损坏是两回事，不能都表现为「返回空」：取消时不应弹出错误提示，出错时应当弹出。

**`selectImport` 返回空同样有两种含义，由诊断区分**：记录了 `Error` 表示用户根本无法被询问（例如文件中没有任何可选条目），属于失败；未记录 `Error` 表示用户拒绝，不属于失败。这一约定是必需的：若二者混淆，`AutomaticSelector` 报告的「文件中没有可导入的内容」会被 `read()` 当作用户取消，导致一个无法读取的文件不产生任何提示。

```cpp
struct ImportResult {
    std::optional<Project> project;   // 为空表示未导入
    bool cancelled = false;           // 为空的原因是用户取消，而非出错
    QList<Diagnostic> diagnostics;
};
```

### 注册表

`InterchangeRegistry` 是一张表，**内置格式与插件使用同一注册途径**，不区分主次。文件对话框的过滤器、按扩展名查找驱动、「导入为…」菜单均由该表生成。

stdcorelib 的 `StaticRegistry` / `DynamicRegistry` 正适用于此，但**它是私有依赖，不得出现在公开头文件中**。`InterchangeRegistry` 是 hellokit 自身的类型，需要使用 stdcorelib 时仅在 `.cpp` 中使用。

### 命名

`hellokit` 使用扁平命名空间 `hello::kit`，而非每个子库一个命名空间，因此类名在整个 kit 中必须唯一。这就是此处所有公共类型都带 `Interchange` 前缀的原因，而不使用 `Reader`、`Source`、`Entry` 这类在目录内看似足够、放到命名空间中终将冲突的名称。

## 自定义选择步骤

部分格式的选择步骤无法由 `optionSchema()` 的几种控件组合而成，此时驱动可以提供自己的界面页。

**第一个使用者是 MIDI 的编码选择**，它无法用一个下拉框解决：左侧为编码列表，右侧须同时预览轨名、歌词和标记，切换编码时三部分预览全部重新解码并刷新，用户依据哪一栏出现乱码来判断选择是否正确。这正是无法放入 schema 的步骤。

**难点在于驱动位于 kit 中，而 kit 不链接 QtWidgets。** 因此一个自定义步骤分为两部分，通过 ID 对应：

| 部分 | 位置 | 职责 |
|---|---|---|
| `InterchangeReader::customStepId()` | `HelloKitInterchange` | 声明需要自定义界面页，并给出 ID |
| `InterchangeStepPage` 的实现 | `HelloUtauEditor` 或插件的 widgets 一侧 | 界面页本身 |

```cpp
// HelloUtauEditor 一侧
class HELLOUTAU_EDITOR_EXPORT InterchangeStepPage : public QWidget {
    Q_OBJECT
public:
    explicit InterchangeStepPage(QWidget *parent = nullptr);
    ~InterchangeStepPage() override;

    /// 将源文件的信息显示在界面上。
    virtual void reset(const InterchangeSource &source) = 0;

    /// 将界面上的选择写入 request，返回 false 表示该页尚未填写完整。
    virtual bool apply(ImportRequest &request) const = 0;
};

class HELLOUTAU_WIDGETS_EXPORT InterchangeStepRegistry {
public:
    using Factory = std::function<InterchangeStepPage *(QWidget *parent)>;

    void registerStep(const QString &id, Factory factory);
    InterchangeStepPage *create(const QString &id, QWidget *parent) const;
};
```

规则如下：

- **自定义页取代的是生成的表单，而非 `optionSchema()` 本身。** 驱动仍然声明其选项，因为该表是**键名与默认值的声明**，自定义页只是填写同一组键的另一种界面。若不声明，`AutomaticSelector` 将无法获取默认值，无界面导入也将无法确定编码。
- **界面上二者择一，不并存。** 一个驱动要么使用生成的表单，要么使用自定义页。若选项分散在两处，用户难以查找，维护成本也过高。
- **它是一页，而非整个对话框。** 条目选择、确定与取消、诊断展示等公共部分仍由导入对话框提供，以保证各驱动之间的一致性。
- **找不到已注册的页时退回通用表单**，并记录一条诊断。只安装了 kit 一侧的插件应当仍然可用，而不是无法打开。

### 默认编码可以推测，最终编码不可以

编码页打开时的默认选中项允许通过启发式规则确定：**先尝试 UTF-8，若解码出现非法字符则退回系统编码**。

这与 [`AGENTS.md`](../AGENTS.md) 中「不要猜测编码，也不要以检测代替记录」并不冲突，因为推测的是**默认选中项**，而非最终结果。用户仍会对照预览确认或更改，确认后该编码即被记录。禁止的是将检测结果直接作为答案、不经用户确认就继续执行。

## 线程

**接口不规定线程，但实现方必须处理。** 若导入移至后台线程，`InterchangeSelector` 的回调将在工作线程上被调用，而 Qt 的对话框只能在 GUI 线程中打开。

第一版导入在调用线程中同步执行，不得自行创建线程。确需后台执行时，widgets 一侧的实现须通过 `BlockingQueuedConnection` 切换回 GUI 线程。这一点须写入接口的 `\warning`。

## 应用层的导入流程

**本节描述的内容不属于本模块**，写在此处是为了说明边界的位置。

取得 `Project` 之后，应用须询问用户插入位置：

| 位置 |
|---|
| 插入到当前选区之后 |
| 插入到当前选区之前 |
| 替换当前选区 |
| 新建工程 |

**这一选择不经由 `InterchangeSelector`。** 该接口存在的理由是「驱动需要用户的回答才能继续」，编码和轨道选择属于此类，插入位置则不属于：驱动自始至终不使用这一答案。让它进入该接口，等于让 kit 知晓「当前选区」这种纯编辑器状态。

约束如下：

- **前两条约束中所述的取舍在这一层进行。** 是否保留 tempo、是否裁掉前导休止符都在此决定，默认值随插入位置而定：新建工程时默认保留前导休止符（以保持小节位置），插入时默认裁掉。
- **插入时丢弃全部工程级设置**（轨名、音源目录、flags），但**必须记录一条诊断**，不得静默丢弃。
- **插入必须是一个撤销步骤。** 导入产出的音符整批提交给撤销栈，不能逐个追加。

## MIDI 导入

**MIDI 导入的优先级高于所有其他格式。**

UTAU 本体带有 MIDI 导入功能，但**其实现有缺陷，HelloUtau 不予照搬**。这一点须特别记录，因为 [`AGENTS.md`](../AGENTS.md) 中规定「功能对齐时以 v0.4.18 的行为为准」，若不说明，日后可能有人以该规定为由将 HelloUtau 的实现改回 UTAU 的行为。

**判据是导出的工程本身正确，而非与 UTAU 逐音符一致。**

### 两份前作

同一作者曾两次实现这一功能，**后一次比前一次正确得多**：

| | 位置 | 定位 |
|---|---|---|
| QSynthesis（2021，已停止维护） | `.cache/QSynthesis-Old/QSynthesis/Frontend/Utils/FilePasers/FilePasers_Midi.cpp` | 转换逻辑的结构参考，但有五处缺陷，见下文 |
| qsynthesis-revenge 的 `iemgr` 插件 | `src/plugins/diffscope/iemgr/`，导入对话框位于 `Internal/Utils/private/ImportDialog_p.cpp` | **只参考思路，不照搬代码。** 该对话框本身设计欠佳 |

**沿用的是其处理编码问题的思路，而非其实现。** 本文档中的以下几点出自该处：条目选择与编码选择分为两个标签页而非一个表单、编码标签页按需显示、条目携带原始字节并由界面即时解码、切换编码时三部分预览同时刷新、解码失败显示为「解码失败」而非乱码、先尝试 UTF-8 失败后退回系统编码、选满后再次选择时替换最早选中的条目。

`iemgr` 是 DiffScope 的一个**插件**，而非内置模块。这也是「格式转换适合实现为插件」这一判断的出处。

### MIDI 解析使用 wolf-midi

`wolf-midi`（vcpkg 端口）是去除 Qt 依赖后的 `QMidiFile`，基于同一份 David Slomin 的代码，接口使用 `std::filesystem::path` 和 `std::vector<char>`，不依赖 Qt。`HelloKitInterchange` 私有链接它，其头文件不出现在公开头文件中。

### 单声部化不是取舍

MIDI 的和弦与重叠音符进入 UST 时必须简化，因为 **UST 无法表示同时发声**。这与「是否保留 tempo」之类的取舍性质不同：后者是目标格式能够表示、由用户偏好决定的；前者是目标格式的硬性限制，没有选择余地。因此简化在本模块中进行，规则如下：

- **同时开始的音符只保留最高的一个。** 旋律通常位于最高声部。
- **重叠的音符将前一个截短至后一个的起点。** 两个音符均得以保留，而前作将后一个整个丢弃，导致缺少一个音。
- 两种情况均逐条计数并报告诊断。

### QSynthesis 实现中不可沿用的部分

`.cache/QSynthesis-Old/QSynthesis/Frontend/Utils/FilePasers/FilePasers_Midi.cpp` 是同一作者的前作实现，可作为结构参考，但以下五点属于缺陷而非行为特征：

| | 其做法 | 问题 |
|---|---|---|
| 编码 | 歌词和轨名均使用 `QString::fromLocal8Bit` | 将编码问题转嫁给运行环境。中文系统读取日文 MIDI 时直接出现乱码。编码只能由用户选择，这正是 `optionSchema()` 中 `encoding` 的用途 |
| NoteOff 配对 | 按下标配对 `noteOffs.at(i)`，无法配对时默认为 480 tick | 不考虑音高和通道，任何含重叠或交错音符的 MIDI 都会错位 |
| 歌词匹配 | 按 tick 精确相等匹配音符起点 | 相差一个 tick 即静默丢弃整条歌词 |
| 重叠音符 | `prevTick > start` 时整个丢弃 | UST 确实是单声部，必须简化，但「丢弃」是几种做法中损失最大的一种，且不产生诊断 |
| 多轨选择 | 对话框使用多选控件，代码只取第一个选中项 | 界面与行为不一致 |

值得保留的结构：列出轨名、音符数、音域供用户选择；音高钳制到 `[24, 107]`（C1–B7）；`tick / resolution * 480` 的换算；空隙补 `R`。

## 插件分类

**`InterchangePlugin` 是第五类插件**，[`docs/note.md`](note.md) 已将其列为「格式转换插件」。
