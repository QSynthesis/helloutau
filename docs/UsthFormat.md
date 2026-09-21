# `.usth` 格式规格

HelloUTAU 的工程文件。JSON，UTF-8，**不带 BOM**，换行 `\n`。

本文档是格式的权威定义。产品层面的说明见 [`note.md`](note.md)，实测依据见 [`claude/utau-ust-preservation.md`](claude/utau-ust-preservation.md)。

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
| `$format` | string | 恒为 `"usth"`。用来在扩展名不可信时辨认文件 |
| `version` | int | 格式版本，当前为 1 |
| `settings` | object | 整个工程共有的设置 |
| `tracks` | array | 轨道。**第 1 版长度必须为 1**，其余情况读取时报错 |

`tracks` 从一开始就是数组，是为了将来放开多轨时不必改 `version`。读取方遇到长度不为 1 的文件应当明确报错，而不是默默只取第一条——静默丢数据比打不开更糟。

未知的顶层字段**原样保留并写回**。这让旧版本的 HelloUTAU 打开新版本的文件时不至于把东西吃掉。

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
| `tempo` | double | `Tempo` | 起始速度，音符可以改变它 |
| `flags` | string | `Flags` | 工程级 flags，音符的 flags 追加在其后 |
| `outputFile` | string | `OutFile` | |
| `cacheDir` | string | `CacheDir` | 缓存目录。UTAU 存盘时会把它改写成跟随文件名 |
| `wavtool` | string | `Tool1` | 见下 |
| `resampler` | string | `Tool2` | 见下 |
| `mode2` | bool | `Mode2` | 音高用 Mode2 曲线还是 Mode1 采样数组 |

### 引擎路径是存下来的，但不被信任

`wavtool` 和 `resampler` 在 UTAU 里是逐工程可配的，不同工程用不同的重采样器是真实的用法。**所以它们原样存、原样写回**，丢掉等于主动删用户的设置。

危险的不是存，是不问就执行。规则在 `AGENTS.md` 的安全底线里，这里复述一遍：**从工程文件读到的引擎路径默认不使用**，渲染时用本地配置里的引擎，除非用户在明确的提示里选择信任这一份工程自带的。空串表示工程没有指定，那就直接用本地配置。

`patch` 和 `userData` 也是同样的道理——文件里来的东西照单存下，执行与否是另一回事。

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

UST 的 `VBR` 有第八个值，UTAU 不用它，本格式用一个 `intensity` 字段原样带着它。

**不能像原先设想的那样丢进 `userData`**：`userData` 里的东西导出时会各自写成一个独立条目，而这个值是 `VBR` 的一部分，那样会在 UST 里造出一个假条目。

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

`values` 是每 5 tick 一个读数，起点由 `start` 给出。类型是 double。

UST 里写成空的读数读回来是 0，不区分「这里没有」——stdutau 就是这么处理的，往返能承诺的也只有这些。

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

键名保留 UST 里的原样，包括 `$` 前缀。**值在这里是 UTF-8 字符串**，读入 UST 时按那个文件的编码转过来。

## 编码

**`.usth` 自身永远是 UTF-8，本格式没有编码字段。** 内存里也只有 UTF-8，见 `AGENTS.md`。

编码只在和 `.ust` 打交道时才存在。

### UTAU 的 `Charset` 表达不了编码

UST 里的 `Charset` 只认两种取值：空，或者 `UTF-8`。它说的是「是不是 UTF-8」，不是「是哪种编码」。所以一份 Shift_JIS 的 UST 和一份 GBK 的 UST 在 UTAU 眼里长得一模一样，都是「没写 `Charset`」。

**这就是控制音符必须携带编码的原因。** 载荷是 base64url 的纯 ASCII，可以在决定整个文件怎么解码之前先按字节扫出来，正好补上 `Charset` 说不出的那一半。

### 导出

编码**可配置**，是应用设置，导出时还能逐次改。默认 UTF-8：UTAU 从 0.4.10 起支持 UTF-8 的 UST（0.4.11 修掉了当时那个会让 `utau.exe` 不定期崩溃的读写缺陷），而 Shift_JIS 表示不了中文歌词和中文文件名，一落到转义串就不好看也不好用。要把文件交给还在用老插件老工具的人时，改成 Shift_JIS。

导出 UTF-8 时写 `Charset=UTF-8`，导出别的编码时不写这一行，因为写不出来。无论哪种，实际用的编码都记在控制音符的载荷里。

### 读入

1. 控制音符的载荷里有 `ustCharset` 就按它。这是唯一权威的来源。
2. 没有载荷，但 `[#VERSION]` 写着 `Charset=UTF-8`，就按 UTF-8。
3. 都没有，要求用户选择。

选定的编码把整个文件转成 UTF-8，之后不再有第二种编码。

### 另外两条路径

**给插件的 `temp.ust`** 按插件自己声明的编码写，见 `note.md` 的插件一节。不要拿导出设置去写插件的临时文件。

**音源配置**（`oto.ini` 等）看各目录自己的 `hello-config.json`。

目标编码表示不了的字符用转义串，规则见 `note.md`。这只在目标不是 UTF-8 时用得上。

## 与 `.ust` 的互转

### 导出

1. 写 `[#VERSION]`：`UST Version1.2`，导出 UTF-8 时再加 `Charset=UTF-8`。
2. 写 `[#SETTING]`：`Tempo`、`Tracks=1`、`ProjectName`、`VoiceDir`、`OutFile`、`CacheDir`、`Tool1`、`Tool2`、`Mode2`、`Flags`，都取工程里的值。`wavtool` 和 `resampler` 为空时写本地配置里的引擎路径，否则 UTAU 打开这份 UST 会找不到引擎。
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
  "ustCharset": "Shift_JIS"
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `version` | int | 载荷版本，当前为 1 |
| `ustCharset` | string | 这份 UST 实际用的编码。`Charset` 说不出的那一半 |

未知的载荷字段**原样保留并写回**，所以将来往里加东西不必改 `version`。逐工程的插件状态预计会加到这里，但**在想清楚要存什么之前不进规格**。

用一个条目而不是多个，是因为值的长度没有实际上限（实测 65536 字符原样保留），拆开只会多出拼装的麻烦。

**载荷必须是纯 ASCII。** 它在文件里的位置比文件的编码更靠前——扫描的时候还不知道整个文件怎么解码，只能按字节找。base64url 正好满足，而且载荷内部的 JSON 是 UTF-8，与外层文件的编码无关。

条目名用 `$` 前缀是硬性的——**UTAU 只保留音符段里 `$` 开头的未知条目**，不带 `$` 的、以及放在 `[#SETTING]` 或 `[#VERSION]` 里的，一律丢弃。这是实测结论。

### 导入

1. **先按 ASCII 扫描找 `$usth=`**，有就解码载荷。这一步必须在解码整个文件之前做，因为编码正是从这里得来的。
2. 定编码：载荷里的 `ustCharset` 优先，其次 `Charset=UTF-8`，都没有就要求用户选择。按该编码把整个文件转成 UTF-8。
3. 控制音符**不进 `notes`**，它带的东西已经在第 1 步取走了。
4. 其余音符依次进 `tracks[0].notes`，未知条目进各自的 `userData`。

**导出加一个控制音符，导入恰好吃掉一个。** 不配平的话反复往返会在开头累积出一串 0.5 秒的前导。

### 遇到不认识的段

UTAU 会把它不认识的段落**当成音符**，而不是忽略——实测 `[#HUPDATA]` 被转成了 `Length=15`、`NoteNum=24`、空歌词的垃圾音符，后面所有音符索引顺移一位。

所以 **HelloUTAU 写 `.ust` 时绝不能造自定义段落**。

读的时候**静默跳过**，交给 stdutau 的 `UstFile::read()` 处理，`[#PREV]` / `[#NEXT]` 这些插件临时文件才有的段也一样。

代价是我们的音符索引会和 UTAU 的解释差一位——UTAU 把那个段当成了音符，我们没有。但报错会让一份还能用的文件直接打不开，而这种文件多半来自别的编辑器，不是真的损坏。

## 往返承诺

分两层，不要混为一谈。

**HelloUTAU 专有信息无损。** 用户在 UTAU 中编辑导出的 UST，只要不动控制音符，保存出的新 UST 能完整转换回 `.usth`。

**工程本身只保证 UTAU 能等价打开，不保证字节无损。** UTAU 存盘会归一化数值、省略等于默认值的条目、重算 `@preuttr` 一族的只读量。判据是语义等价：两边各自解析成内存模型，规范化后比较。
