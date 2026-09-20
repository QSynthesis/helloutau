# 格式转换模块 `HelloKitInterchange`

本文档规定 `hellokit/lib/Interchange/` 的职责边界与接口形状。产出目标 `HelloKitInterchange`，include 路径 `<hellokit/Interchange/...>`，命名空间 `hello::kit`。

这个模块在 [`Roadmap.md`](Roadmap.md) 里对应第六阶段那条「MIDI / VSQ / SynthV 导入」，但 **MIDI 导入的优先级被提到了最前面**，见文末。

## 边界

**`HelloKitDocument` 管我们自己的格式，`HelloKitInterchange` 管别人的格式。**

| | 归谁 |
|---|---|
| `.usth`、`.ust` | `HelloKitDocument` |
| MIDI 进出、VSQ / VSQX 进、SynthV `.svp` 进、OpenUTAU `.ustx` 进 | `HelloKitInterchange` |
| `presamp`、`frq` | `HelloKitVoiceBank` |

`presamp` 和 `frq` 在 Roadmap 里和 MIDI 列在同一行，那是按「兼容周边」分的，不是按模块分的。这两样描述的是音源不是工程。

**依赖方向是 `HelloKitInterchange` → `HelloKitDocument`，不能反过来。** 转换的中间物就是 Document 的 `Project`，Interchange 只负责把别人的东西翻译成它。

## 三条硬约束

### 一、忠实翻译，不做取舍

**Interchange 只把文件里有的东西翻译出来，不判断用户要不要。** 取舍属于应用层的导入流程，不属于这个模块。

两个具体后果：

- **tempo 原样翻译。** 首个 tempo 进 `settings.tempo`，中途的 tempo 进对应音符的 `note.tempo`。应用层不想要就自己抹掉。
- **前导休止符照补。** 第一个音符之前那段静默在源文件里是真实存在的，翻译就该把它翻出来。插入到现有工程时要裁掉是应用层的事，只有它知道插在哪。

这条不是风格偏好。它决定了哪些东西**不该**出现在 `ImportRequest` 里：任何形如「要不要保留 X」的选项都说明取舍被写进了错误的层。

### 二、有损是常态，所以诊断必须是返回值的一部分

MIDI 没有歌词以外的任何 UTAU 参数，VSQ 的参数和 UTAU 不是一一对应，源文件里的多声部进 UST 必须化简。**转换丢东西不是缺陷是必然，但静默丢是缺陷。**

所以每次转换都产出一份诊断列表，能定位到具体音符，由应用层展示。写进日志不算交代。

`Diagnostic` 类型**不属于这个模块**。音源扫描、合成、工程读取都要报同类的东西，它应当是 `HelloKitSupport` 里的公共类型，Interchange 只是第一个用它的。

### 三、`hellokit` 不链接 QtWidgets

接口里不许出现对话框。要问用户的事情通过 `InterchangeSelector` 回调出去，由链 QtWidgets 的那一侧实现。

## 接口

### 探查与条目

```cpp
/// 文件里的一条可导入的东西：MIDI 的一条 track、VSQ 的一个 part、ustx 的一条轨。
struct InterchangeEntry {
    int index;
    int noteCount;
    std::optional<int> lowestNote;    // 音域，给界面显示
    std::optional<int> highestNote;

    // 编码还没定，所以这里是未解码的字节。见下。
    QByteArray rawName;
    QList<QByteArray> rawLyrics;
};

/// 探查的结果，在转换任何东西之前就能拿到。
struct InterchangeSource {
    QString formatId;
    QList<InterchangeEntry> entries;
    QList<QByteArray> rawLabels;
};
```

`inspect()` 是公开的，因为「只想看看这文件里有什么」是独立需求（文件对话框预览），不该被迫走一遍导入。

### 为什么这里是未解码的字节

`rawName` 和 `rawLyrics` 是 `QByteArray` 不是 `QString`，这是**故意的，而且是这个模块里唯一一处**。

编码是用户在这个界面上选的，选之前没人知道该怎么解。要让用户看着预览选编码，界面就必须拿到原始字节、按当前选中的编码当场解码。先在 `inspect()` 里解好再交出去，等于在用户还没回答之前替他答了。

这是 [`AGENTS.md`](../AGENTS.md) 那条「原始字节不许离开 I/O 边界那一层」的一处**有界例外**，边界划在这儿：

- 只有 `InterchangeSource` 这一个结构能带原始字节。
- **`read()` 返回的 `Project` 里一个字节都不许有**，那时编码已经定了，全部是 UTF-8。
- 换句话说，原始字节只存在于「还没选定编码」这个窗口里，窗口一关就没了。

**解码失败要显示出来，不要显示成乱码。** 解不出来的条目在预览里明确写「解码失败」，用户一眼能看见这个编码选错了。

### 驱动

读和写是两个纯虚类，**不合并成一个带 capability 掩码的**。大部分格式只能进不能出，一个接口加掩码的话「不支持的那半」只能返回 false 靠文档约束，拆开则由类型系统管。一个驱动两个都实现就都注册。

```cpp
class HELLOKIT_INTERCHANGE_EXPORT InterchangeReader {
public:
    virtual ~InterchangeReader();

    virtual QString id() const = 0;             // "midi"
    virtual QString name() const = 0;           // "Standard MIDI File"
    virtual QStringList suffixes() const = 0;   // {"mid", "midi"}

    /// 这个驱动认哪些选项。界面照着生成控件，AutomaticSelector 照着取默认值。
    virtual QList<InterchangeOption> optionSchema() const { return {}; }

    /// 需要自己的选择步骤时返回它的 id，空表示用 optionSchema() 生成的通用表单。
    virtual QString customStepId() const { return {}; }

    /// 探查，不转换。
    virtual std::optional<InterchangeSource> inspect(const std::filesystem::path &path,
                                                     DiagnosticList &diagnostics) = 0;

    /// 整条流程：探查、问 selector、转换。非虚，驱动不实现这个。
    ImportResult read(const std::filesystem::path &path, InterchangeSelector *selector,
                      const ImportLimits &limits = {});

protected:
    /// 问题都答完之后，按 request 转换。由 read() 调用。
    virtual std::optional<Project> convert(const std::filesystem::path &path,
                                           const InterchangeSource &source,
                                           const ImportRequest &request,
                                           DiagnosticList &diagnostics) = 0;
};
```

方法名是 `read` / `write` 而不是 `import` / `export`，因为 **`export` 是 C++ 关键字**。

**`read()` 是非虚的，驱动只实现 `inspect()` 和 `convert()` 两块。** 探查、问 selector、转换这三步的编排写在基类里，因为它一旦让调用方自己拼，就多出一个可以拼错顺序的地方，而命令行、测试和编辑器本该以完全相同的方式导入。

### 选项分两层

```cpp
struct InterchangeOption {
    QString key;                  // "encoding"
    QString name;                 // 显示用
    enum Type { Bool, Int, Enum, Text } type;
    QVariant defaultValue;
    QStringList choices;          // Enum 用
};

struct ImportRequest {
    QList<int> entries;           // 选哪几条，所有格式共通
    QVariantMap driverOptions;    // 驱动自己声明的那些，key 见 optionSchema()
};
```

**「选哪几条」保持强类型，不进 schema。** 它每个格式都有，而且界面要显示的东西比一个下拉框多得多——轨名、音符数、音域。塞进 `QVariantMap` 会把这些信息碾平。

**其余选项一律由驱动自己声明。** 文本编码就是典型：MIDI 是古早格式，歌词和轨名都没有编码声明，只能问用户；而后来的 `.ustx`、`.svp`、`.vsqx` 都是 Unicode，根本没有这个问题。把 `encoding` 放进公共结构等于让三个格式背一个只有第四个需要的字段。

**但编码本身不走这张 schema，它是自定义步骤的第一个客户**，理由见下一节。schema 适合的是「一个控件问一件事」的选项，比如「音符短于多少 tick 就丢弃」。

### 向用户提问

```cpp
/// 文件没说、只能问人的那些事。实现在链 QtWidgets 的那一侧。
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

/// 目的地能装下多少，由调用方给。
struct ImportLimits {
    int minEntries = 1;
    int maxEntries = 1;    // 工程现在单轨，所以是 1
};
```

一次问完，不拆成「选轨」「选编码」两个回调。拆了界面上就是连弹两个对话框。驱动通过 `reader` 参数被读到 `optionSchema()`。

**能选几条是调用方说的，不是驱动说的。** 驱动只知道文件里有几条，不知道目的地装得下几条——工程现在单轨，所以 `maxEntries` 是 1，将来放开多轨时改的是调用方，驱动一行不动。界面在**选满之后再选**时应当把最早选的那条顶掉，而不是拒绝用户的点击。

**`nullptr` 不能等于卡住或者崩。** kit 里内置一个 `AutomaticSelector`：照 `optionSchema()` 取每一项的默认值，选中全部条目，并且**每替用户做一次决定就记一条 `Note` 级诊断**。命令行工具、测试、将来的批量转换都用它，传 `nullptr` 就是用它。这样测试永远不会挂在一个等不到答案的对话框上。

### 取消不是失败

用户点叉和文件损坏是两回事，不能都表现成「返回空」——取消了不该弹错误框，出错了该弹。

**`selectImport` 返回空也有两种意思，靠诊断区分**：记了 `Error` 是「这个问题根本问不出来」（比如文件里没有任何可选的条目），是失败；没记 `Error` 就是用户拒绝，不是失败。这条不是可有可无的约定——写第一版实现时这里就错了，`AutomaticSelector` 报「文件里没东西可导」被 `read()` 当成了用户取消，结果一个读不了的文件静悄悄什么都不说。测试抓到的。

```cpp
struct ImportResult {
    std::optional<Project> project;   // 空就是没导进来
    bool cancelled = false;           // 空的原因是用户取消，不是出错
    QList<Diagnostic> diagnostics;
};
```

### 注册表

`InterchangeRegistry` 一张表，**内置格式和插件走同一条注册路径**，没有二等公民。文件对话框的过滤器、按扩展名找驱动、「导入为…」菜单，全从这张表生成。

stdcorelib 的 `StaticRegistry` / `DynamicRegistry` 正好是干这个的，但**它是私有依赖，不能出现在公开头文件里**。`InterchangeRegistry` 是 hellokit 自己的类型，要用 stdcorelib 就在 `.cpp` 里用。

### 命名

`hellokit` 是一个扁平命名空间 `hello::kit`，不是每个子库一个，所以类名要在整个 kit 里唯一。这就是为什么这里所有公共类型都带 `Interchange` 前缀，而不是 `Reader`、`Source`、`Entry` 这种在目录内看着够用、放到命名空间里迟早撞车的名字。

## 自定义选择步骤

有些格式的选择步骤没法用 `optionSchema()` 那几种控件拼出来。这时驱动可以自带一页界面。

**第一个客户就是 MIDI 的编码选择**，它不是一个下拉框能解决的：左边是编码列表，右边要同时预览轨名、歌词和标记，切一下编码三块预览全部重解码刷新，用户靠看哪一栏变成乱码来判断选对没有。这正是不能塞进 schema 的那种步骤。

**难点在于驱动住在 kit 里，而 kit 不链 QtWidgets。** 所以一个自定义步骤是两半，用 id 对上：

| 半边 | 住哪 | 干什么 |
|---|---|---|
| `InterchangeReader::customStepId()` | `HelloKitInterchange` | 声明「我要一页自己的界面」，给出 id |
| `InterchangeStepPage` 的实现 | `HelloUtauWidgets` 或插件的 widgets 那一侧 | 那一页界面本身 |

```cpp
// HelloUtauWidgets 一侧
class HELLOUTAU_WIDGETS_EXPORT InterchangeStepPage : public QWidget {
    Q_OBJECT
public:
    explicit InterchangeStepPage(QWidget *parent = nullptr);
    ~InterchangeStepPage() override;

    /// 把源文件的情况铺到界面上。
    virtual void reset(const InterchangeSource &source) = 0;

    /// 把界面上的选择写进 request，返回 false 表示这一页还没填完。
    virtual bool apply(ImportRequest &request) const = 0;
};

class HELLOUTAU_WIDGETS_EXPORT InterchangeStepRegistry {
public:
    using Factory = std::function<InterchangeStepPage *(QWidget *parent)>;

    void registerStep(const QString &id, Factory factory);
    InterchangeStepPage *create(const QString &id, QWidget *parent) const;
};
```

三条规矩：

- **自定义页取代通用表单，不与它并存。** 一个驱动要么全用 schema，要么全自己画。一半选项在这儿一半在那儿，用户找不到，我们也维护不动。
- **它是一页，不是整个对话框。** 条目选择、确定取消、诊断展示这些公共部分仍然由导入对话框提供，各驱动之间保持一致。
- **找不到注册的页就退回通用表单**，并记一条诊断。一个只装了 kit 那半边的插件应当仍然可用，而不是打不开。

### 默认编码可以猜，最终编码不可以

编码页打开时选中哪一项，允许用一个启发式来定：**先试 UTF-8，解码出现非法字符就退回系统编码**。

这和 [`AGENTS.md`](../AGENTS.md) 里「不要猜编码，也不要用检测代替记录」不冲突，因为猜的是**默认选中项**，不是最终结果。用户仍然看着预览确认或改掉，确认之后那个编码就被记下来。禁止的是拿检测结果当答案、不问用户就往下走。

## 线程

**接口不规定线程，但实现方必须管。** 导入要是挪到后台线程，`InterchangeSelector` 的回调就在工作线程上被调用，而 Qt 的对话框只能在 GUI 线程开。

第一版导入就在调用线程同步跑，不要自作主张开线程。真要后台跑，widgets 那侧的实现自己用 `BlockingQueuedConnection` 弹回 GUI 线程。这条要写进接口的 `\warning`。

## 应用层的导入流程

**这一节描述的东西不在这个模块里**，写在这儿是为了说清楚边界落在哪。

拿到 `Project` 之后，应用要问用户放哪儿：

| 位置 |
|---|
| 插入到当前选区之后 |
| 插入到当前选区之前 |
| 替换当前选区 |
| 新建工程 |

**这个选择不走 `InterchangeSelector`。** 那个接口存在的理由是「驱动需要有人回答才能往下走」，编码和选哪条轨是这种，插入位置不是——驱动从头到尾用不上这个答案。让它进去就等于让 kit 知道「当前选区」这种纯编辑器状态。

约束三条：

- **前两条约束里说的取舍在这一层做。** tempo 要不要、前导休止符裁不裁，都由这里决定，并且默认值跟着插入位置走：新建工程默认保留前导休止符（保住小节位置），插入默认裁掉。
- **工程级设置在插入时全部丢弃**（轨名、音源目录、flags），但**要留一条诊断**，不要静默。
- **插入必须是一步撤销。** 导入产出的音符整批交给撤销栈，不能一个一个 append。

## MIDI 导入

**MIDI 导入的优先级排在所有格式之前。**

UTAU 本体带 MIDI 导入，但**它的实现有缺陷，我们不照抄**。这条要特别写下来，因为 [`AGENTS.md`](../AGENTS.md) 里有「做功能对齐时要对齐的是 v0.4.18 的行为」，不说清楚的话，以后会有人拿那句话当理由把我们的实现改回去。

**判据是导出的工程本身正确，不是和 UTAU 逐音符一致。**

### 两份前作

同一作者写过两遍这件事，**后一遍比前一遍对得多**：

| | 哪儿 | 定位 |
|---|---|---|
| QSynthesis（2021，停更） | `.cache/QSynthesis-Old/QSynthesis/Frontend/Utils/FilePasers/FilePasers_Midi.cpp` | 转换逻辑的形状参考，但有五处缺陷，见下 |
| qsynthesis-revenge 的 `iemgr` 插件 | `src/plugins/diffscope/iemgr/`，导入对话框在 `Internal/Utils/private/ImportDialog_p.cpp` | **只参考思路，不要照搬代码。** 那个对话框本身设计得不好 |

**要拿的是它对编码问题的处理思路，不是它的实现。** 本文档里这几条出自那里：条目选择和编码选择是两个 tab 而不是一个表单、编码 tab 按需出现、条目带原始字节由界面当场解码、切换编码三块预览一起刷新、解码失败显示成「解码失败」而不是乱码、先试 UTF-8 失败退回系统编码、选满之后再选顶掉最早的那条。

`iemgr` 是 DiffScope 的一个**插件**，不是内置模块。这也是「格式转换适合做成插件」这个判断的出处。

### QSynthesis 那份里不能抄的

`.cache/QSynthesis-Old/QSynthesis/Frontend/Utils/FilePasers/FilePasers_Midi.cpp` 是同一作者的前作实现，可以当形状参考，但下面五条是缺陷不是行为特征：

| | 它做了什么 | 为什么不行 |
|---|---|---|
| 编码 | 歌词和轨名都走 `QString::fromLocal8Bit` | 把编码问题拖到了运行环境上。中文系统读日文 MIDI 直接乱码。编码只能问用户，这正是 `optionSchema()` 里 `encoding` 的用处 |
| NoteOff 配对 | 按下标配对 `noteOffs.at(i)`，配不上就默认 480 tick | 不看音高和通道。任何有重叠或交错的 MIDI 都会错位 |
| 歌词匹配 | 按 tick 精确相等匹配音符起点 | 差一个 tick 整条歌词静默丢弃 |
| 重叠音符 | `prevTick > start` 时整个丢掉 | UST 确实是单声部必须化简，但「丢掉」是几种做法里最有损的一种，而且没有诊断 |
| 多轨选择 | 对话框是多选控件，代码只取第一个选中项 | 界面和行为对不上 |

值得保留的形状：列出轨名、音符数、音域让用户选；音高钳到 `[24, 107]`（C1–B7）；`tick / resolution * 480` 的换算；空隙补 `R`。

## 待定

**`InterchangePlugin` 是第五类插件，[`docs/note.md`](note.md) 需要相应修改。** 那份文档现在定死了四类（原版 UTAU 插件、`RangeEditPlugin`、`EditorExtensionPlugin`、`VoiceBankPlugin`），而 note.md 是产品形态的唯一权威，改它要先跟作者确认。在确认之前，本文档里关于插件的部分是提案不是定论。
