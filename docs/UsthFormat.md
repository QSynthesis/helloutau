# `.usth` 格式规格

HelloUtau 的工程文件格式：JSON，UTF-8，**不带 BOM**。默认写成紧凑形式，不含缩进与换行，因为缩进会使音符多的工程体积增大数倍；需要阅读或比较文件时可以写成缩进形式，换行符为 `\n`。读取时两种形式都接受。下文的示例为便于阅读写成缩进形式。

本文档是该格式的权威定义。产品层面的说明见 [`note.md`](note.md)，实测依据见 [`claude/utau-ust-preservation.md`](claude/utau-ust-preservation.md)。

## 顶层

```json
{
  "$format": "usth",
  "version": 1,
  "settings": { },
  "tracks": [ { } ]
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `$format` | string | 恒为 `"usth"`，用于在扩展名不可信时识别文件 |
| `version` | int | 格式版本，当前为 1 |
| `settings` | object | 工程级设置 |
| `tracks` | array | 轨道。**第 1 版中长度必须为 1**，其他长度在读取时报错 |

`tracks` 从一开始就是数组，以便将来开放多轨时无需修改 `version`。读取方遇到长度不为 1 的文件必须明确报错，而不能静默地只取第一项，因为静默丢失数据比无法打开更严重。

未知的顶层字段**原样保留并写回**，以免旧版本的 HelloUtau 打开新版本的文件时丢失数据。

## `settings`

```json
{
  "name": "",
  "tempo": 120.0,
  "flags": "",
  "outputFile": "",
  "cacheDir": "",
  "wavtool": "",
  "resampler": "",
  "mode2": true
}
```

| 字段 | 类型 | 对应 UST | 说明 |
|---|---|---|---|
| `name` | string | `ProjectName` | |
| `tempo` | double | `Tempo` | 起始速度，可被音符更改 |
| `flags` | string | `Flags` | 工程级 flags，音符的 flags 追加在其后 |
| `outputFile` | string | `OutFile` | |
| `cacheDir` | string | `CacheDir` | 缓存目录。UTAU 保存时会将其改写为随文件名变化 |
| `wavtool` | string | `Tool1` | 见下文 |
| `resampler` | string | `Tool2` | 见下文 |
| `mode2` | bool | `Mode2` | 音高使用 Mode2 曲线还是 Mode1 值数组 |

### 引擎路径的保存与信任

`wavtool` 和 `resampler` 在 UTAU 中可逐工程配置，不同工程使用不同的重采样器是实际存在的用法。**因此它们被原样保存并原样写回**，丢弃它们等于主动删除用户的设置。

危险不在于保存，而在于未经询问即执行。规则见 `AGENTS.md` 的安全底线，此处复述如下：**默认不使用从工程文件中读取的引擎路径**，渲染时使用本地配置的引擎，除非用户在明确的提示中选择信任该工程自带的引擎。空字符串表示工程未指定引擎，此时直接使用本地配置。

`patch` 和 `userData` 同理：来自文件的内容原样保存，是否执行是另一回事。

## `tracks[]`

```json
{
  "name": "",
  "voiceDir": "%VOICE%uta",
  "notes": [ ]
}
```

| 字段 | 类型 | 对应 UST | 说明 |
|---|---|---|---|
| `name` | string | —— | 轨道名，UST 中无对应项，导出时丢弃 |
| `voiceDir` | string | `VoiceDir` | 音源目录，`%VOICE%` 前缀表示共享音源目录 |
| `notes` | array | `[#NNNN]` | 见下文 |

## `notes[]`

**音符按顺序排列，其位置由之前所有音符（包括休止符）的长度累加得出。** 不存储绝对位置。

这一设计是为了保证精确往返：UST 采用相同的排列方式，而休止符在 UST 中是可以携带条目的真实音符。若改用绝对位置，导出时就必须生成新的休止符，原休止符上携带的内容将会丢失。编辑器在内存中的表示方式另作考虑，文件按 UST 的方式排列。

```json
{
  "lyric": "あ",
  "length": 480,
  "noteNum": 60,

  "intensity": 100,
  "modulation": 0,
  "velocity": 100,
  "preUtterance": null,
  "voiceOverlap": null,
  "startPoint": null,
  "tempo": null,
  "flags": "",

  "envelope": null,
  "vibrato": null,
  "portamento": [ ],
  "pitchBend": null,

  "label": "",
  "direct": "",
  "patch": "",
  "region": "",
  "regionEnd": "",

  "userData": { }
}
```

### 必需字段

| 字段 | 类型 | 对应 UST |
|---|---|---|
| `lyric` | string | `Lyric`。`R`、`r` 和空字符串表示休止符 |
| `length` | int | `Length`，单位为 tick，480 为一个四分音符 |
| `noteNum` | int | `NoteNum`，24 为 C1 |

### 可选字段

以下字段**缺省时写为 `null` 或直接省略**，二者等价，均表示「文件未指定」，这与「已指定且值为 0」不同。它们对应 `stdutau` 中为空的 `std::optional`。

| 字段 | 类型 | 对应 UST | 缺省时的行为 |
|---|---|---|---|
| `intensity` | double | `Intensity` | 使用 100 |
| `modulation` | double | `Modulation` | 使用 100 |
| `velocity` | double | `Velocity` | 使用 100 |
| `preUtterance` | double | `PreUtterance` | 使用音源 `oto.ini` 中的值 |
| `voiceOverlap` | double | `VoiceOverlap` | 同上 |
| `startPoint` | double | `StartPoint` | 使用 0 |
| `tempo` | double | `Tempo` | 沿用前一个音符的速度 |

`flags` 为空字符串表示没有 flags，与 UST 中的行为一致。

### 参数

| 字段 | 类型 | 对应 UST |
|---|---|---|
| `envelope` | object 或 null | `Envelope` |
| `vibrato` | object 或 null | `VBR` |
| `portamento` | array | `PBS` / `PBW` / `PBY` / `PBM`，Mode2 音高 |
| `pitchBend` | object 或 null | `PBStart` / `PitchBend`，Mode1 音高 |

工程处于 Mode2 或 Mode1 之一，因此 `portamento` 和 `pitchBend` 最多只有一个非空。

```json
"envelope": {
  "anchors": [ {"x": 0, "y": 0}, {"x": 5, "y": 100}, {"x": 35, "y": 100}, {"x": 0, "y": 0} ]
}
```

四个或五个锚点，按时间顺序排列。第五个锚点是可选的中间锚点，存在时位于索引 2。锚点为其他个数时，读取方报告一条警告并忽略该包络，不拒绝整个文件。

```json
"vibrato": {
  "length": 65, "period": 180, "amplitude": 35,
  "attack": 20, "release": 20, "phase": 0, "offset": 0
}
```

UST 的 `VBR` 有第八个值，UTAU 不使用它，本格式以一个 `intensity` 字段原样保存该值。

**该值不能存入 `userData`**：`userData` 中的每一项在导出时各自写为一个独立条目，而该值属于 `VBR` 的一部分，存入 `userData` 会在 UST 中产生一个虚假条目。

```json
"portamento": [
  {"x": -40, "y": 0, "type": "S"},
  {"x": 10, "y": 100, "type": "Linear"}
]
```

即 UST 中的 `PBS=-40;0`、`PBW=50`、`PBY=10`、`PBM=s`：第二个点在第一个点之后 50 毫秒，即音符起点之后 10 毫秒，高一个半音。

`x` 是距音符起点的毫秒数，每个点都如此，可为负值，表示延伸至前一个音符。`y` 的单位为音分。UST 的写法不同：`PBS` 给出第一个点，`PBW` 给出其后各点与前一个点的间隔，高度以十分之一个半音为单位；读写 UST 时换算。`type` 取值为 `"S"`、`"Linear"`、`"R"`、`"J"`，分别对应 `PBM` 中的空字符串、`s`、`r`、`j`。

**使用全称而非字母。** `PBM` 中的字母与类型名不对应（`s` 表示 Linear，空字符串才表示 S），本格式没有理由再次沿用这一易错的对应关系。

```json
"pitchBend": { "start": -20, "values": [0, 10, 20] }
```

`values` 为每 5 tick 一个值，起点由 `start` 指定，类型为 double。

UST 中为空的值读取后为 0，不区分「此处无值」。stdutau 按此处理，往返所能保证的也仅限于此。

### 其他字段

| 字段 | 类型 | 对应 UST |
|---|---|---|
| `label` | string | `Label` |
| `direct` | string | `$direct` |
| `patch` | string | `$patch` |
| `region` / `regionEnd` | string | `$region` / `$region_end` |

`patch` 是来自工程文件的路径，**视为不可信输入**，见 `AGENTS.md` 的安全底线。

### `userData`

UST 音符上所有在本格式中没有对应字段的条目，以键名到值的映射形式原样保存，并在导出时写回音符段的末尾。这是 `.ust` → `.usth` → `.ust` 能够无损的原因。

键名保持 UST 中的原样，包括 `$` 前缀。**值在此处为 UTF-8 字符串**，读取 UST 时按该文件的编码转换。

## 编码

**`.usth` 本身始终为 UTF-8，本格式没有编码字段。** 内存中也只有 UTF-8，见 `AGENTS.md`。

编码问题只在与 `.ust` 交互时出现。

### `Charset` 的表达能力限制

UST 中的 `Charset` 只有两种取值：空，或 `UTF-8`。它表示的是「是否为 UTF-8」，而非「具体是哪种编码」。因此一份 Shift_JIS 的 UST 和一份 GBK 的 UST 在 UTAU 看来完全相同，均为「未写 `Charset`」。

**这就是控制音符必须记录编码的原因。** 载荷是 base64url 编码的纯 ASCII，可以在确定整个文件的解码方式之前按字节扫描得到，恰好补足 `Charset` 无法表达的信息。

### 导出

编码**可以配置**，属于应用设置，每次导出时也可单独更改。默认为 UTF-8：UTAU 从 0.4.10 起支持 UTF-8 编码的 UST（0.4.11 修复了当时会导致 `utau.exe` 不定期崩溃的读写缺陷），而 Shift_JIS 无法表示中文歌词和中文文件名，转义后的文本既不易读也不便使用。需要将文件交给仍在使用旧插件和旧工具的用户时，可改为 Shift_JIS。

导出为 UTF-8 时写入 `Charset=UTF-8`，导出为其他编码时不写该行，因为无法表达。无论采用哪种编码，实际使用的编码都记录在控制音符的载荷中。

### 读取

1. 若控制音符的载荷中有 `ustCharset`，按其解码。这是唯一的权威来源。
2. 若没有载荷，但 `[#VERSION]` 中写有 `Charset=UTF-8`，按 UTF-8 解码。
3. 若两者均无，要求用户选择。

确定编码后，整个文件转换为 UTF-8，此后不再存在第二种编码。

### 其他编码路径

**传给插件的 `temp.ust`** 按插件自身声明的编码写出，见 `note.md` 的插件一节。不得使用导出设置写出插件的临时文件。

**音源配置**（`oto.ini` 等）按各目录自身的 `hello-config.json` 处理。

目标编码无法表示的字符以转义序列表示，规则见 `note.md`。只有目标编码不是 UTF-8 时才会用到。

## 与 `.ust` 的互转

### 导出

1. 写出 `[#VERSION]`：`UST Version1.2`，导出为 UTF-8 时另加 `Charset=UTF-8`。
2. 写出 `[#SETTING]`：`Tempo`、`Tracks=1`、`ProjectName`、`VoiceDir`、`OutFile`、`CacheDir`、`Tool1`、`Tool2`、`Mode2`、`Flags`，均取工程中的值。`wavtool` 和 `resampler` 为空时写入本地配置的引擎路径，否则 UTAU 打开该 UST 时将找不到引擎。
3. 写出控制音符 `[#0000]`，见下文。
4. 依次写出每个音符。
5. 写出 `[#TRACKEND]`。

### 控制音符

歌词恒为 `_USTH_`，`Length` 为 480，`NoteNum` 为 60。

在 UTAU 中它等同于休止符：没有任何音源包含该歌词，因此找不到采样，不发声，不占用渲染时间，但按 `Length` 占据时长。歌词取名醒目，是为了让用户识别出这不是自己写的音符。

载荷为**一个条目**：

```
$usth=<base64url>
```

`<base64url>` 是下列 JSON 的 UTF-8 字节经去除填充的 base64url 编码后的结果，由 `hello::kit::PayloadCodec` 处理：

```json
{
  "version": 1,
  "ustCharset": "Shift_JIS"
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `version` | int | 载荷版本，当前为 1 |
| `ustCharset` | string | 该 UST 实际使用的编码，即 `Charset` 无法表达的信息 |

未知的载荷字段**原样保留并写回**，因此将来添加内容时无需修改 `version`。逐工程的插件状态预计将存放于此，但**在确定具体存储内容之前不纳入规格**。

使用一个条目而非多个，是因为值的长度没有实际上限（实测 65536 个字符被原样保留），拆分只会增加拼装的复杂度。

**载荷必须是纯 ASCII。** 读取时需要在确定文件编码之前定位载荷，此时只能按字节查找。base64url 满足这一要求，而载荷内部的 JSON 为 UTF-8，与外层文件的编码无关。

条目名必须带 `$` 前缀：**UTAU 只保留音符段中以 `$` 开头的未知条目**，不带 `$` 的条目以及位于 `[#SETTING]` 或 `[#VERSION]` 中的条目一律被丢弃。这是实测结论。

### 导入

1. **先按 ASCII 扫描查找 `$usth=`**，若存在则解码载荷。这一步必须在解码整个文件之前进行，因为编码正是从这里获得的。
2. 确定编码：载荷中的 `ustCharset` 优先，其次为 `Charset=UTF-8`，若均无则要求用户选择。按该编码将整个文件转换为 UTF-8。
3. 控制音符**不进入 `notes`**，其携带的内容已在第 1 步中取出。
4. 其余音符依次进入 `tracks[0].notes`，未知条目进入各自的 `userData`。

**导出时添加一个控制音符，导入时恰好移除一个。** 若二者不对应，反复往返会在开头累积一串 0.5 秒的前导音符。

### 未知段落的处理

UTAU 会将无法识别的段落**当作音符**处理，而非忽略：实测中 `[#HUPDATA]` 被转换为 `Length=15`、`NoteNum=24`、歌词为空的无效音符，其后所有音符的索引后移一位。

因此 **HelloUtau 写出 `.ust` 时绝不能创建自定义段落。**

读取时**静默跳过**，交由 stdutau 的 `UstFile::read()` 处理，`[#PREV]` / `[#NEXT]` 等仅出现在插件临时文件中的段落同样如此。

代价是 HelloUtau 的音符索引会与 UTAU 的解释相差一位，因为 UTAU 将该段落当作音符，而 HelloUtau 没有。但报错会导致一份仍可使用的文件无法打开，而这类文件大多来自其他编辑器，并非真正损坏。

## 往返保证

保证分为两个层次，不可混淆。

**HelloUtau 专有信息无损。** 用户在 UTAU 中编辑导出的 UST 时，只要不改动控制音符，保存出的新 UST 就能完整地转换回 `.usth`。

**工程本身只保证 UTAU 能以等价的方式打开，不保证字节级无损。** UTAU 保存时会规范化数值、省略等于默认值的条目、重新计算 `@preuttr` 系列的只读值。判据是语义等价：两侧各自解析为内存模型，规范化后进行比较。
