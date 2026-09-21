# 合成

本文规定 `HelloKitSynth` 的形状。做之前先读 [`note.md`](note.md)（产品形态）和 [`Roadmap.md`](Roadmap.md)（阶段与算数），安全底线在 [`../AGENTS.md`](../AGENTS.md)。

## 已经有的

| 东西 | 干什么 |
|---|---|
| `EngineProcess` | 起一次引擎。参数向量进，**没有接受整条命令行的重载** |
| `SynthPlan` | 只算不跑。`VoiceBank::find` 接到 `utau::Synth::calc`，产出每个音符两条解析好的参数向量 |
| `SynthRunner` | 跑一份 `SynthPlan` |

算和跑分开的理由：真正值得测的部分——歌词解析到哪个样本、相邻音符之间的时值校正、每个引擎到底收到什么——这样不需要磁盘上有引擎就能测。

## 为什么要三套策略

因为「怎么起引擎」不是一个实现细节，是**用户可见的兼容性选择**。

| 策略 | 为什么非有不可 |
|---|---|
| 经典 | **保底。** 有些重采样器依赖 `temp.bat` 存在，甚至读它的内容 |
| 多线程阻塞 | 日常用的那个。重采样互不依赖，时间全花在那里 |
| 实时 | 编辑时边改边听。界面之前只能出模型 |

三者产出的音频应当一致，差的是**怎么调度**和**跑在什么环境里**。所以它们共用 `SynthPlan`，只在「拿到计划之后怎么执行」上分岔。

## 共同的抽象

`SynthRunner` 是基类，三套是它的实现：

```
SynthRunner                 纯虚，render(plan, engines, observer, diagnostics)
├── ClassicSynthRunner       写 temp.bat / temp_helper.bat，起黑窗
├── ThreadedSynthRunner     N 个重采样并发，合成串行
└── RealtimeSynthRunner     按播放位置调度，可打断，可作废
```

**`SynthObserver` 报进度、问取消。** 不是回调函数而是接口，因为实时那套要问的不止「取消了吗」。它会被工作线程调用，实现方自己负责线程安全——这一条和 `InterchangeSelector` 是同一个理由，`hellokit` 不链 QtWidgets，界面那边的事不归这里管。

**引擎路径永远由宿主给，不从工程里取。** `Tool1`、`Tool2` 照原样存，但不照单执行。这条对三套都成立，见 `AGENTS.md`。

## 一、经典（渲染脚本）

### 它是什么

官方 UTAU 在 `%TEMP%\utauX\` 下写批处理再执行。单核时是 `temp.bat` 加 `temp_helper.bat`，多核时是 `temp.bat` 加 `temp1.bat`…`tempN.bat`——**重采样并行跑完，最后才跑 `temp.bat` 做合成**，和第二套策略是同一个形状。

**这一套是跨平台的。** Windows 上写 `temp.bat` 交给命令处理器，其余系统写 `temp.sh` 交给 `/bin/sh`（引擎在那边跑在 Wine 下）。布局一模一样，只有拼法不同：`cat` 代替 `copy /B`，`${var}` 代替 `%var%`，单引号代替 `set "name=value"`。QSynthesis 当年就是这么做的。

选哪一套是 `ScriptShell` 这个**运行时**开关，不是 `#ifdef`——这样两种脚本在任一平台上都能读回来测，写给另一个平台的东西不该等到有人在那边跑才第一次看见。

大致内容（作者自己的[渲染脚本笔记](https://sinestriker.github.io/documents/QPitchEditor/book/Developers/UTAU-Tools/Rendering-Script.html)，**经验总结，不是官方规格**）：

- **页首**：一串 `@set`，把曲速、采样率、音源目录、两个引擎、输出路径、helper 路径、缓存目录、全局 flags、默认包络都存成变量；然后 `@del "%output%"`、`@mkdir "%cachedir%"`
- **正文**：休止符直接 `@"%tool%" "%output%" "%oto%\R.wav" 0 <长度> 0 0`；其余音符先 `@set params/flag/env/stp/vel/temp`，`@echo` 一行进度条，再 `@call %helper% <九个参数>`
- **helper**：`@if exist %temp% goto A` 跳过重采样（这就是缓存复用）→ 跑 resampler → `:A` → 跑 wavtool
- **页脚**：确认 `%output%.whd` 和 `%output%.dat` 都在，`copy /Y ... /B + ... /B` 合成 wav，再删掉两个分片

`wavtool` 不直接写 wav 这件事已经实测确认：它往 `.whd`（44 字节头）和 `.dat`（PCM）里追加，最后拼起来。`SynthRunner` 现在就是这么做的。

### 为什么必须保留

[Wavtool 那页](https://sinestriker.github.io/documents/QPitchEditor/book/Developers/UTAU-Tools/Wavtool.html)提到 **moresampler 会读 `temp.bat` 的内容**来判断自己是不是最后一次调用。也就是说对某些引擎，批处理的**文本本身**是输入，不只是启动方式。不写批处理，这类引擎就不工作。

黑窗也要保留。官方是可见控制台，脚本的输出用户能直接看到；`stdc::Popen` 的 `startupInfo()` 带 `STARTF_USESHOWWINDOW` 就是为这件事改的。

### 两档，默认安全的那档

**这是全仓库唯一一处会把工程里的字符串写进 shell 脚本的地方。** 官方 UTAU 直接拼，歌词或 flags 里有 `&` 或换行就能追加命令——CVE-2024-28886 的形状。

| 档 | 做什么 | 默认 |
|---|---|---|
| `Escaped` | 结构、命令、顺序、文件名、工作目录、黑窗都和官方一致，但赋值一律写成 `@set "name=value"`，值里每个 `%` 加倍 | **是** |
| `Verbatim` | 连不转义一起复刻，`@set name=value` 照官方那样写 | 否 |

选 `set "name=value"` 而不是逐字符 `^` 转义，是因为引号内 cmd 就不再找运算符了，`&`、`|`、`>`、`(` 全部变成字面量，比逐个转义更难出错。`%` 还是要加倍——它的展开发生在引号之前。`!` 不用管，延迟展开默认关着，而音高串 `!120` 正好带这个字符。

**引号和换行进不去。** `"` 会提前结束 `set "name=value"`，换行会结束整行，两者在这个形式里都没有可用的转义。碰上就**报错拒绝**，不写一个残缺的脚本——写歪了比说做不到更糟。

命令行上的每个参数按需加引号（含空格或运算符才加），不是一律加：UTAU 自己也是路径加引号、数字裸着，而这些引擎里有几个是自己解析命令行而不走 C 运行时的，给数字套引号不保证还当数字读。

照抄那档**必须由用户在明确的提示里选择**，提示要说清楚这等于让工程文件决定跑什么命令。不提供「记住我的选择」式的全局开关。

做两档而不是只做安全那档，是因为 moresampler 这类引擎会解析脚本文本，转义改变了文本，可能把它们弄坏——**这一点还没实测**。碰到了就有证据，没碰到就永远用安全档。

## 二、多线程阻塞

重采样之间互不依赖，而且时间全在那里；wavtool 是往同一个文件追加的，必须串行。所以：

```
N 个线程跑重采样  →  全部完成  →  按轨顺序跑 wavtool  →  拼 whd + dat
```

线程数可选（`threadCount`），默认取硬件并发数，用 QtCore 的 `QThreadPool`，不引 QtConcurrent 那个模块。阻塞的意思是调用方等它跑完——界面那边要在工作线程上调，这个类不管。

诊断按**音轨顺序**合并，不按线程完成顺序——后者用户没法读。取消是「不再开新音符，在飞的让它跑完」：中途杀引擎会在缓存里留半个文件，下次渲染就会信它。

缓存复用放在这一套里做：重采样前先看缓存文件在不在。

## 三、实时

界面之前只出模型。要有的东西：

- **按播放位置排队**，先渲染快要播到的音符
- **能作废**：改了一个音符，它和受影响的邻居（先行发声、重叠牵扯到前后）的缓存要失效
- **能打断**：播放位置跳走了，在飞的任务该丢就丢

形状参考 QSynthesis 的 `Frontend/Process/`（`RealtimeRenderer` + `ResampleWork` + `ConcatenateWork`）。**拿它的思想，不拿它的代码**，理由见 `Roadmap.md`。

## 待实测的不一致

下面三处，[作者的笔记](https://sinestriker.github.io/documents/QPitchEditor/book/Developers/UTAU-Tools/Rendering-Script.html)和 stdutau 的实现说法不一样。笔记是经验总结不是官方规格，stdutau 也可能是对的，**没实测过就不要动任何一边**。「和 UTAU 渲染同一工程比对 wav」那套装置建起来以后，这三条是它第一批该回答的问题。

| 处 | 笔记说 | stdutau 做 |
|---|---|---|
| flags 顺序 | 音符 flags 在前、全局在后，同名参数只有第一个有效 | `globalFlags + aFlags`，全局在前 |
| flags 里 `e`/`E` 的转义 | 前面加反斜杠 | `fixFlags` 加的是 `/` |
| 缓存文件名 | `[序号]_[歌词]_[音阶]_[六位随机].wav` | `[序号]_[歌词]_[音阶]_[长度].wav` |

第三条不只是名字问题。六位后缀存在的意义是**音符被改过以后换一个缓存名**；只拿长度做键，改了力度或 flags 会命中旧缓存，渲染出上一次的声音。所以实现缓存复用之前必须先把这条定下来——不然复用就是在制造错误。

## 算数

第二阶段的算数在 [`Roadmap.md`](Roadmap.md)：命令行能把 `.ust` 渲染成 wav（**已过**，见 `tests/manual/ustrender/`），以及和 UTAU 渲染同一工程做比对（还没做）。

比对装置要能重放：同一个工程、同一份音源、同一套引擎，两边各渲染一次，比波形。它是整个项目里「和 UTAU 一致」最硬的一次检验，也是上面那三条不一致的唯一裁判。
