# `.usth` 格式规格

HelloUTAU 的工程文件。JSON，UTF-8，**不带 BOM**，换行 `\n`。

本文档是格式的权威定义。产品层面的说明见 [`note.md`](note.md)，实测依据见 [`claude/utau-ust-preservation.md`](claude/utau-ust-preservation.md)。

## 顶层

```json
{
  "$format": "usth",
  "version": 1,
  "project": { },
  "tracks": [ { } ]
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `$format` | string | 恒为 `"usth"`。用来在扩展名不可信时辨认文件 |
| `version` | int | 格式版本，当前为 1 |
| `project` | object | 整个工程共有的设置 |
| `tracks` | array | 轨道。**第 1 版长度必须为 1**，其余情况读取时报错 |

`tracks` 从一开始就是数组，是为了将来放开多轨时不必改 `version`。读取方遇到长度不为 1 的文件应当明确报错，而不是默默只取第一条——静默丢数据比打不开更糟。

未知的顶层字段**原样保留并写回**。这让旧版本的 HelloUTAU 打开新版本的文件时不至于把东西吃掉。

## `project`

```json
{
  "name": "",
  "tempo": 120.0,
  "flags": "",
  "outputFile": "",
  "mode2": true,
  "ustCharset": "Shift_JIS"
}
```

| 字段 | 类型 | 对应 UST | 说明 |
|---|---|---|---|
| `name` | string | `ProjectName` | |
| `tempo` | double | `Tempo` | 起始速度，音符可以改变它 |
| `flags` | string | `Flags` | 工程级 flags，音符的 flags 追加在其后 |
| `outputFile` | string | `OutFile` | |
| `mode2` | bool | `Mode2` | 音高用 Mode2 曲线还是 Mode1 采样数组 |
| `ustCharset` | string | —— | 导出 `.ust` 时用的编码。见下面「编码」 |

**`Tool1`、`Tool2`、`CacheDir` 不进本格式。**

前两个是引擎路径。把它们存进工程文件等于让工程决定跑什么程序，这正是 CVE-2024-28886 的形状。导出 `.ust` 时写的是本地配置里的引擎路径，不是源文件里的。`CacheDir` 由 UTAU 自己按保存的文件名改写（已实测），记下来也没用。

这意味着 `.ust` → `.usth` → `.ust` 之后这三项会变。这不违反往返承诺：承诺的是「UTAU 能等价打开」，而指向用户本机引擎的路径正是能打开的那一种。

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
| `name` | string | —— | 轨道名，UST 里没有，导出时丢弃 |
| `voiceDir` | string | `VoiceDir` | 音源目录，`%VOICE%` 前缀表示公共音源位置 |
| `notes` | array | `[#NNNN]` | 见下 |

## `notes[]`

**音符是顺序排列的，位置由前面所有音符的长度累加得出，休止符也算在内。** 不存绝对位置。

这样定是为了往返精确：UST 就是这么排的，而休止符在 UST 里是能携带条目的真实音符。改成绝对位置，就要在导出时凭空造休止符，携带在原休止符上的东西会丢。编辑器在内存里怎么表示是另一回事，文件按 UST 的方式排。

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

### 必有的三项

| 字段 | 类型 | 对应 UST |
|---|---|---|
| `lyric` | string | `Lyric`。`R`、`r` 和空串是休止符 |
| `length` | int | `Length`，tick，480 为四分音符 |
| `noteNum` | int | `NoteNum`，24 为 C1 |

### 可缺省的

下面这些**缺省时写 `null` 或直接省略**，两者等价，都表示「文件没有说」，和「说了但值为 0」是两回事。对应 `stdutau` 里的空 `std::optional`。

| 字段 | 类型 | 对应 UST | 缺省时的行为 |
|---|---|---|---|
| `intensity` | double | `Intensity` | 用 100 |
| `modulation` | double | `Modulation` | 用 100 |
| `velocity` | double | `Velocity` | 用 100 |
| `preUtterance` | double | `PreUtterance` | 用音源 `oto.ini` 里的值 |
| `voiceOverlap` | double | `VoiceOverlap` | 同上 |
| `startPoint` | double | `StartPoint` | 用 0 |
| `tempo` | double | `Tempo` | 沿用前一个音符的速度 |

`flags` 为空串表示没有，UST 里的行为一致。

### 参数

| 字段 | 类型 | 对应 UST |
|---|---|---|
| `envelope` | object 或 null | `Envelope` |
| `vibrato` | object 或 null | `VBR` |
| `portamento` | array | `PBS` / `PBW` / `PBY` / `PBM`，Mode2 音高 |
| `pitchBend` | object 或 null | `PBStart` / `PitchBend`，Mode1 音高 |

工程处在 Mode2 或 Mode1 其中一种，所以 `portamento` 和 `pitchBend` 只会有一个非空。

```json
"envelope": {
  "anchors": [ {"x": 0, "y": 0}, {"x": 5, "y": 100}, {"x": 35, "y": 100}, {"x": 0, "y": 0} ]
}
```

四个或五个锚点，按时间顺序。第五个是可选的中间锚点，有它的时候它排在索引 2。

```json
"vibrato": {
  "length": 65, "period": 180, "amplitude": 35,
  "attack": 20, "release": 20, "phase": 0, "offset": 0
}
```

UST 的 `VBR` 有第八个值，UTAU 不用它，读写时原样带着但不出现在本格式里——它进 `userData`。

```json
"portamento": [
  {"x": -40, "y": 0, "type": "S"},
  {"x": 50, "y": 10, "type": "Linear"}
]
```

`x` 是毫秒，第一个点相对音符起点（可为负，表示伸进前一个音符），其余相对前一个点。`y` 是十分之一个半音。`type` 取 `"S"`、`"Linear"`、`"R"`、`"J"`，对应 `PBM` 里的空串、`s`、`r`、`j`。

**写全称不写字母。** `PBM` 里的字母和类型名对不上（`s` 是 Linear，空串才是 S），在本格式里再背一次这个坑没有道理。

```json
"pitchBend": { "start": -20, "values": [0, 10, 20] }
```

`values` 是每 5 tick 一个读数，起点由 `start` 给出。

### 其余

| 字段 | 类型 | 对应 UST |
|---|---|---|
| `label` | string | `Label` |
| `direct` | string | `$direct` |
| `patch` | string | `$patch` |
| `region` / `regionEnd` | string | `$region` / `$region_end` |

`patch` 是从工程文件来的路径，**当作不可信输入**，见 `AGENTS.md` 的安全底线。

### `userData`

UST 音符上一切本格式没有表示的条目，键名到值的映射，原样保留并在导出时写回音符段尾。这是 `.ust` → `.usth` → `.ust` 能无损的原因。

键名保留 UST 里的原样，包括 `$` 前缀。**值在这里是 UTF-8 字符串**，从 UST 读入时按 `project.ustCharset` 转过，导出时转回。

## 编码

`.usth` 自身永远是 UTF-8，不需要记录自己的编码。`project.ustCharset` 记的是**关联的 `.ust` 用什么编码**，用于导出，以及用户第一次打开 `.ust` 时选定后记住。

目标编码表示不了的字符用转义串，规则见 `note.md`。

## 与 `.ust` 的互转

### 导出

1. 写 `[#VERSION]`（`UST Version1.2`，需要时加 `Charset=`）。
2. 写 `[#SETTING]`：`Tempo`、`Tracks=1`、`ProjectName`、`VoiceDir`、`OutFile`、`CacheDir`、`Tool1`、`Tool2`、`Mode2`、`Flags`。**`Tool1`、`Tool2` 和 `CacheDir` 来自本地配置，不来自工程。**
3. 写控制音符 `[#0000]`：见下。
4. 依次写每个音符。
5. 写 `[#TRACKEND]`。

### 控制音符

歌词恒为 `_USTH_`，`Length` 为 480，`NoteNum` 为 60。

它在 UTAU 里等价于休止符——没有任何音源有这个歌词，所以查不到采样、不发声、不占渲染时间，但按 `Length` 占时长。名字取得醒目是为了让用户看出这不是自己写的音符。

载荷是**一个条目**：

```
$usth=<base64url>
```

`<base64url>` 是下面这个 JSON 的 UTF-8 字节经 base64url 去填充编码后的结果，由 `hello::kit::PayloadCodec` 处理：

```json
{
  "version": 1,
  "ustCharset": "Shift_JIS",
  "plugins": { }
}
```

用一个条目而不是多个，是因为值的长度没有实际上限（实测 65536 字符原样保留），拆开只会多出拼装的麻烦。

**载荷必须是纯 ASCII**，这不是风格问题：读取方要先把文件当 ASCII 扫描、找到 `$usth=`、解出编码，才谈得上解码整个文件。base64url 正好满足。

条目名用 `$` 前缀是硬性的——**UTAU 只保留音符段里 `$` 开头的未知条目**，不带 `$` 的、以及放在 `[#SETTING]` 或 `[#VERSION]` 里的，一律丢弃。这是实测结论。

### 导入

1. 按 ASCII 扫描找 `$usth=`。找到就解码载荷，取得 `ustCharset`；找不到就要求用户选择编码。
2. 按该编码解析整个文件。
3. 控制音符**不进 `notes`**，它的内容进 `project` 和插件配置。
4. 其余音符依次进 `tracks[0].notes`，未知条目进各自的 `userData`。

**导出加一个控制音符，导入恰好吃掉一个。** 不配平的话反复往返会在开头累积出一串 0.5 秒的前导。

### 遇到不认识的段

UTAU 会把它不认识的段落**当成音符**，而不是忽略——实测 `[#HUPDATA]` 被转成了 `Length=15`、`NoteNum=24`、空歌词的垃圾音符，后面所有音符索引顺移一位。

所以：HelloUTAU 写 `.ust` 时绝不能造自定义段落；读到不认识的段名时当作**文件已损坏**处理并告知用户，不能跳过。跳过会和 UTAU 的解释产生分歧，而用户手里那份已经被改过了。

## 往返承诺

分两层，不要混为一谈。

**HelloUTAU 专有信息无损。** 用户在 UTAU 中编辑导出的 UST，只要不动控制音符，保存出的新 UST 能完整转换回 `.usth`。

**工程本身只保证 UTAU 能等价打开，不保证字节无损。** UTAU 存盘会归一化数值、省略等于默认值的条目、重算 `@preuttr` 一族的只读量。判据是语义等价：两边各自解析成内存模型，规范化后比较。
