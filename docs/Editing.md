# 编辑层 `HelloKitEdit`

本文档规定 `hellokit/lib/Edit/` 的职责边界与接口形状。产出目标 `HelloKitEdit`，include 路径 `<hellokit/Edit/...>`，命名空间 `hello::kit`。依赖 `HelloKitDocument` 与 `HelloKitVoiceBank`。

在 [`Roadmap.md`](Roadmap.md) 里，这是第三阶段「编辑器骨架」的**第一步**，排在窗口和钢琴卷帘之前。

## 一句话

**编辑发生在一棵节点树上。改树只有一条路：调用会话上那组类型化的函数，函数自己记账，所以撤销、日志、通知都不是补上去的。渲染和存盘时从树物化出一个 `Project`。命令是那组函数的文本外壳。**

## 为什么先做这个

界面写完再补撤销栈，撤销栈就永远是补的：总有几个入口绕过它，而绕过的那几个正是最难复现的 bug。反过来，先把「修改」这件事定死，界面就只剩「把鼠标动作翻译成函数调用」一件事。

还有一个当下就兑现的好处：**编辑语义在没有界面的时候就能测**。

## 边界

| | 归谁 |
|---|---|
| 文件长什么样、怎么读怎么写 | `HelloKitDocument` / `HelloKitVoiceBank` |
| 编辑、撤销、崩溃恢复 | **`HelloKitEdit`** |
| 改完拿去发声 | `HelloKitSynth` |
| 鼠标怎么变成修改 | `HelloUtauWidgets` |

`HelloKitEdit` 不认识界面，不链接 QtWidgets。

## 事实来源：树是真的

编辑期间，**树是文档**。`Project` 和 `VoiceBank` 仍然是普通值类型，负责读写文件和交给合成层——但它们是**从树物化出来的快照**，不是被编辑的对象。

```
                    ┌──────────────┐
   .ust / .usth ───▶│              │───▶ Project ───▶ SynthPlan
                    │     树       │      （快照）
   界面 / 命令  ◀──▶│  （会话独占） │───▶ Project ───▶ 存盘
                    └──────────────┘
```

**规矩：界面读树，合成读快照。** 渲染本来就该渲一个快照——一次渲染要几秒，期间用户还在改，读快照是对的。

### 这条选择的代价，和为什么还是选它

代价是**两份表示**，转换是新的漂移点。守法是往返测试，而这个形状我们已经有并且信得过：`.usth` ↔ `Project` 就是这么守的，`ustconv --check` 那套判据直接复用。

换来的是节点模型的全部好处——身份、属性与子节点统一、嵌套寻址天然、将来接 [substate](https://github.com/stdware/substate) 是直接映射而不是翻译。这些东西**不选树也得自己造一遍**，那才是最贵的走法。

现有代码**一行不动**：222 处 `Note` 字段访问、两个读写器、`SynthPlan::make(const Project &)`、MIDI 转换、所有手动工具和测试，全部照旧。新增的只有树的定义和一层双向转换。

## 节点模型

三种节点，外加一个「槽位里装什么」的概念。命名刻意跟 substate 对齐，将来换后端时对应关系一眼可见。

| 节点 | 语义 |
|---|---|
| `StructNode` | **固定槽位**的记录。槽位表编译期定，按下标寻址 |
| `VectorNode` | **有序**子节点列表，按下标 `insert` / `remove` / `move` |
| `MappingNode` | key → 值 |

```cpp
/// 一个槽位里装的东西：一个标量，或一个子节点，或什么都没有。
class Value {
public:
    enum Kind { Empty, Scalar, Child };
    // …
};
```

**`Value` 把「属性」和「子节点」统一成同一个槽位机制。** 于是「改一个标量」和「换一个子节点」是同一种操作，记账只有一种形状。`vibrato` 是不是 `null`，就是那个槽位装着子节点还是 `Empty`。

substate 还有 `SheetNode`（自增 id 的表）和 `BytesNode`，我们现在用不上，等需要了再加——**别提前实现用不到的节点类型**。

### 身份

每个节点有一个 `NodeId`，在会话内稳定：插入、删除、撤销、重做都不改变已存在节点的 id。

**界面拿 id，寻址用下标。** 正在拖的那个音符、选中的那几个控制点，记的是 id；`/tracks/0/notes/12` 这种路径是给命令和日志用的。下标会因为插入删除平移，id 不会。

id **不存盘**。打开文件时重新生成，因为它是会话内的引用，不是文档的内容。

## 模型往树上套

| 我们的 | 节点 | |
|---|---|---|
| `Project` | Struct | settings / tracks |
| `ProjectSettings` | Struct | 全标量槽 |
| `tracks` | Vector | 长度恒为 1 |
| `Track` | Struct | name / voiceDir / notes |
| `notes` | **Vector** | 见下 |
| `Note` | Struct | 标量槽 + 子节点槽（envelope / vibrato / pitchBend）|
| `portamento` | Vector | 控制点有序 |
| `PortamentoPoint` | Struct | x / y / type |
| `userData` / `unknownFields` | Mapping | 键是 UST 给的名字 |
| `VoiceBank` | Struct | character / samples / prefixMap |
| `samples` | Mapping | 键是文件名 |
| 一个 wav 的 oto 条目 | Vector | 一个文件多个别名，有序 |
| `prefixMap` | Mapping | 键是音阶 |

### 音符为什么用 Vector 而不是带 start 的集合

**因为 R 不是空白。** UST 里的休止符能带 `Tempo`（变速点就挂在它上面）、`Label`、以及 `$` 开头的自定义条目。把休止符吸收成「空隙」会把这些丢掉，直接违反 UST 往返语义相等那条承诺。

顺序即时间，跟 UST 的存储形式一一对应，存盘零转换。代价是「把音符往后拖 100 tick」不是改一个字段，而要动相邻休止符的长度——那是界面层的事，封装成一个领域函数即可。丢数据不是。

`VectorNode` 的元素仍然各有 id，所以**有序和有身份不冲突**。

## 三层

```
命令（文本）        note set 0 12 lyric a
      │            只是外壳，没有独占能力
      ▼
领域函数            transpose / quantize / insertNote / splitNote …
      │            多；组合下面那一层；一次调用包在一个事务里
      ▼
节点操作            set 槽位 / insert / remove / move
                   少，类型化，**记账发生在这里**
```

### 节点操作：少，但是类型化

槽位不是裸下标。schema 把每个槽位声明成一个带类型的句柄：

```cpp
namespace NoteSlots {
    inline constexpr Slot<QString>              Lyric     {0, "lyric"};
    inline constexpr Slot<int>                  Length    {1, "length"};
    inline constexpr Slot<int>                  NoteNum   {2, "noteNum", Range{0, 127}};
    inline constexpr Slot<std::optional<double>> Intensity{3, "intensity", Range{0, 200}};
    // …
}
```

于是一个 `set` 模板就够，调用点仍然编译期有类型：

```cpp
session.set(note, NoteSlots::Lyric, QStringLiteral("a"));      // 好
session.set(note, NoteSlots::NoteNum, QStringLiteral("abc"));  // 编译不过
session.set(vibrato, VibratoSlots::Period, 180.0);             // 嵌套一样自然
```

**不写二十个 `setNoteLyric` / `setNoteLength` 之类的包装。** 槽位表同时是：类型、名字（日志和命令层要用）、约束（见下）。加一个字段只改一处。

序列和表：

```cpp
session.insert(notes, 12, std::move(newNotes));
session.remove(notes, 12, 3);
session.move(notes, 12, 3, 20);
session.set(userData, "$Custom", value);
session.remove(userData, "$Custom");
```

### 领域函数：多，组合上面那层

`insertNote`、`splitNote`、`transpose`、`quantize`、`setTempoAt`、`normalizeEnvelopes`……数量不封顶，因为它们是产品功能。每一个把若干节点操作包进**一个事务**，因而是**一步撤销**。

领域函数不自己记账——它下面调的节点操作已经记了。

## 五种形状

记账是通用的，按形状各写一遍，不是每个函数写一遍：

| 形状 | 记什么 | 逆 |
|---|---|---|
| 槽位赋值 | before, after | 写回 before |
| 序列插入 | 区间 | 删掉这个区间 |
| 序列删除 | 删掉的子树 | 插回去 |
| 序列移动 | 区间与目标 | 反向移动 |
| 表项增删改 | 键，before，after | 写回 before |

**哪天有个操作落不进这五种，先别急着加第六种——大概率是那个操作该拆。**

## 事务

```cpp
{
    auto tx = session.transaction(tr("移动 3 个音符"));
    session.remove(notes, 12, 3);
    session.insert(notes, 20, std::move(moved));
}   // 析构时提交；中途返回或抛出则回滚
```

消息就是界面上「撤销：移动 3 个音符」那一句，也是无头模式日志里打的那一行。

## 校验分两处

**属性级约束（范围、格式、枚举）声明在槽位表里**，上面 `Range{0, 127}` 就是。一处定义，节点操作和命令层共用同一份——命令层还拿它生成帮助文本和校验参数。加字段时约束跟着字段走，不会漏。

**跨字段、跨节点的不变量放在事务边界**，不在函数里。包络锚点的顺序、portamento 的 x 递增、同一个 wav 下 oto 别名不撞车，这类东西**中间态合法地非法**：一次「把三个音符往后挪」是先删后插，删完那一刻过不了检查，逐函数校验会误报。

所以照数据库的做法：**约束延迟到提交时求值**。事务内允许中间态，提交前跑一遍受影响节点的 `validate()`，不过就整体回滚。RAII 事务本来就要在异常和提前返回时回滚，走同一条路径。

还有第三种值得提但现在不做：**让非法状态无法表示**（`noteNum` 用带范围的类型）。最强，但读别人的 UST 时确实会遇到越界值，读入层得能容纳非法值再报诊断，类型一收紧这条路就堵了。

## 命令

`<名词> <动词> [参数…]`，一行一条，每条落到**一个事务**。

```
note insert 0 12 --lyric a --length 480 --note C4
note remove 0 12 3
note set    0 12 lyric "a i"
tempo set   134.0
oto set     a.wav 0 preUtterance 8.938
```

引号规则**只有一条**：双引号包起来，里面用反斜杠转义双引号和反斜杠。不支持单引号、反引号、变量展开。一条规则够用，多了就是下一个 bug 的温床。

### 路径寻址

嵌套深的地方用路径更顺手：

```
set /tracks/0/notes/12/vibrato/period 180
set /tracks/0/notes/12/intensity null
```

`/` 分隔，每段要么是槽位名要么是十进制下标。**路径里永远不出现数据**——歌词、flags、文件名只会作为*值*出现。所以路径不需要转义，也不会因为歌词里有空格或斜杠就解析错。

`std::optional` 的字段用 `null` 表示「文件没说」。`set …/intensity null` 和 `set …/intensity 100` 是两回事，UST 里也确实是两回事。

### 变更日志：JSON Lines

给机器看的那一份用 JSON Lines，转义规则现成，什么字符都不怕，插件和外部工具生成它也容易：

```json
{"shape":"set","node":41,"slot":"lyric","before":"a","after":"i"}
{"shape":"insert","node":17,"index":12,"count":1}
```

### 命令不是脚本语言

没有变量、条件、循环、表达式求值。需要那些的时候答案是插件。

### 选择区不进命令层

参数总是显式的。界面把「当前选中的音符」翻译成显式参数再发。这样一串命令的意思不依赖于执行时谁被选中，脚本才能重放，测试才能写。

## 音源是第二种文档

从第一天就一起设计。音源的形状和工程很不一样，这正是要一起设计的原因：一个 wav 有**多个** oto 条目，一份音源有**多个目录各自一份 `oto.ini`**，而且**每个目录的编码可能不同**。

### 读写已经就绪

`VoiceBank` 按目录保存：每个目录留着读它用的编码、自己的 `character.txt` / `prefix.map` / `readme.txt`，每个样本知道自己属于哪个目录。**合并视图就是 `samples()`，分目录视图就是按目录筛它**，两种编辑模式看的是同一份数据。编辑层物化出来的 `VoiceBank` 直接 `save()`，下面这些它已经保证了，编辑层不用再管：

- **只写和读进来时不一样的文件。** 比较的是「现在编码出来的字节」和「打开时编码出来的字节」，不是和磁盘比——LF 换行、没排序的文件，没人动就不会被重写。
- **按各目录自己的编码写。** 编码装不下的字符拒绝，**一次列出全部**，不写成 `?`。
- **磁盘上被别人改过的文件拒绝替换**（UTAU 的 setParam 可能同时开着）。先全部检查再动手，失败就一个文件都不动。
- **没读成的目录（`leftOut`）和有字节没解开的目录（`lossy`）不写**，否则解不开的那些会被写成空。
- 写一个目录就把它的编码记进 `hello-config.json`；没动的目录不会凭空多一个文件。
- `hasEntry == false` 的样本（靠文件名唱的）不会变成一行。
- 数字保留原来的写法（`41` 和 `41.0` 在同一份文件里都有）；改过的条目整行按最短写法重写。
- `oto.ini` 存盘按文件名排序。**UTAU 存盘也排**，所以没有音源指望自己的顺序能活过一次保存。

**「设置编码」是两个操作，界面要分开给：**

| | 字节 | 文字 | 用在 |
|---|---|---|---|
| **转换**：`setDirectory()` 改 `charset`，再 `save()` | 变 | 不变 | 想改成 UTF-8 之类 |
| **重新解读**：`reread()` | 不变 | 变 | 当初选错了编码、读出乱码，或者没选、目录被跳过了 |

转换之前问 `VoiceBank::isCharsetReadableByUtau()`：答否就警告**原版 UTAU 在这台机器上读不回来**，`oto.ini` 里的非 ASCII 文件名也会找不到 wav。重新解读丢掉该目录没存的改动，这是它的意思所在，不是副作用。打开时用户答过的编码用 `rememberCharset()` 让下次存盘记下来，否则每次打开都要再问一遍。

## 日志接缝

```cpp
class EditJournal {
public:
    virtual ~EditJournal();
    virtual bool append(const Transaction &) = 0;
    virtual std::optional<Transaction> undo() = 0;
    virtual std::optional<Transaction> redo() = 0;
    /// 上一次存盘之后没落地的事务，按顺序。
    virtual std::vector<Transaction> recover() = 0;
};
```

**编辑层一个 `ss::` 类型都不提。**

第一版是内存实现。这不是权宜之计——内存里的撤销栈本来就是每个编辑器都有的东西，substate 加的是**持久**那一半。

### 为什么现在不接 substate

它那边：`FilesystemStorageEngine` 的 public 区目前只有一行 `// TODO`，`tests/` 下只有一个 CMakeLists 没有测试——崩溃一致性还没实现，而且作者说它要重新洗牌。

我们这边：日志是最可能变的部分，也是语义含量最低的部分；而「音符是什么、函数意味着什么」这些难的、值钱的东西，不该跟着日志一起动。这和 `SynthRunner::makeEngineProcess()` 是同一招。

节点模型既然照着 substate 的形状定，将来换后端是把我们的 `StructNode` / `VectorNode` / `MappingNode` 映射到它的同名节点，**不是重新发明**。

### 恢复

恢复 = **上一次存盘的文件 + 重放未落地的事务**。日志只存事务，底层不需要理解树的语义。

## 无头工具

`hellokit/tests/manual/ustedit`，和 `ustconv`、`ustrender` 并列：

```sh
ustedit song.usth --script edits.txt -o out.usth
ustedit song.usth                  # 从 stdin 读命令
ustedit song.usth --dump-changes   # 把命令展开成变更打出来，不执行
```

## 算数

1. **往返**：`Project` → 树 → `Project`，两边逐字段相等。这一条守着物化层，是选 B 的全部代价所在。
2. 无头模式能打开真实工程，执行一串命令，撤销到底再重做到底，存回去，**UTAU 能正常打开**且内容符合预期。
3. 撤销到底之后存出来的文件，与没执行任何命令时存出来的**语义相等**（`ustconv --check` 那套判据）。
4. 同一串命令跑两遍，两次的变更序列逐条相同。
5. 节点 id 在插入、删除、撤销、重做之后仍然指向同一个节点。
6. 音源侧：改一条 oto 条目、存回去，**编码不变**，没动过的条目一个字节都不变。
7. **每个节点操作和领域函数都有对应的命令，每条命令都落到函数上。** 用一个两边列出来对照的测试守住，新增函数忘了加命令就红。

第 3 条是这一层最硬的检验：撤销栈只要有一处逆写错，它就红。

## 不做什么

- **多轨。** UST 是单轨，`.usth` 的 `tracks` 数组长度恒为 1，见 [`UsthFormat.md`](UsthFormat.md)。树里留着 `/tracks/0/` 这一层，将来放开时不动语法。
- **协同编辑。** 不在视野里，别为它设计。
- **把命令做成脚本语言。**
- **提前实现用不上的节点类型。** `SheetNode`、`BytesNode` 等需要了再加。
- **让记账那一层理解业务。** 「量化」「移调」是领域函数，不是形状。形状只认槽位、序列和表。
