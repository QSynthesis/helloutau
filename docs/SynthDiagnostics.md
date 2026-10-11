# 渲染失败的诊断

本文规定渲染失败时的诊断：在报告「这个音符无法渲染」之外，给出可能的原因与解决办法。作者 2026-10-11 提出要求，诊断器可由插件注册。渲染本身的结构见 [`Synth.md`](Synth.md)，插件的注册方式见 [`Plugins.md`](Plugins.md)。

## 动机

渲染失败时，用户现在看到的是合成工具的输出原文，或者「重采样器没有输出」。原因往往不在音符，而在合成工具的配置、文件路径或系统环境，新手无从判断。诊断器根据失败时的现场判断原因，并给出可以照做的解决办法。

## 失败现场

诊断需要的信息在渲染过程中产生，现在只留下一句消息与音符编号。渲染器为每个失败或可疑的步骤记录一个 `SynthFailure`，放入 `SynthOutcome::failures`：

| 字段 | 内容 |
|---|---|
| `stage` | 重采样（`Resampler`），合成（`Wavtool`），或分片不可读（`Fragment`） |
| `noteIndex` | 音符编号，整轨的合成失败时为空 |
| `program`、`arguments` | 运行的程序与参数 |
| `run` | `SynthToolRun`：是否启动、退出码、是否超时、输出文本 |
| `outputFile` | 期望的输出文件 |
| `outputSize`、`outputHead` | 输出文件的大小与开头的字节，文件不存在时大小为 -1 |

**「成功」的渲染也可能可疑。** 渲染器以分片文件是否存在判断重采样是否成功，因为合成工具的退出码不可靠（见 `SynthToolRun::exitCode`）。但重采样器可能写出一个不是 wav 的分片并以 0 退出（见下文 moresampler 的实测）。分片不是 RIFF/WAVE、且之后的拼接不由同一个程序完成时，渲染器同样记录一个 `Fragment` 阶段的现场，即使整轨已经写出。拼接由同一个程序完成时，该程序可能使用自己的中间格式，这样的分片不视为可疑。

成功的渲染只做最少的检查（作者 2026-10-11 决定）：只读取本次新写出的分片的文件头，不检查复用的分片，也不运行任何诊断器。只有渲染失败或发现不是 wav 的分片时，才运行诊断器。

三种渲染器都记录现场（`SynthOutcome::failures`，实时模式为 `RealtimeSynth::takeFailures()`）：

- 多线程模式记录每次重采样的完整结果。因 `stopOnFirstFailure` 而没有运行的音符不记录，否则会被误诊为程序未能启动。合成器未能启动，或者整轨没有写出，记为 `Wavtool`；没有任何一次合成器调用时，整轨缺失的原因已由各次重采样的现场说明，不另记。
- 经典模式由批处理运行合成工具，取不到单次调用的结果，现场的 `run` 为空，只按分片文件逐个检查。批处理本身未能启动时，现场的程序是批处理文件而不是合成器。
- 实时模式以 `WavtoolMixer` 读取每个分片，读不了的分片无论合成器是什么都记录。

## 诊断器

`kit::SynthDiagnoser`（`HelloKitSynth`）读取一次渲染的全部现场，返回零个或多个 `SynthDiagnosis`。一次读取全部现场，是因为同一原因常使整轨失败，应报告一次而不是每个音符各报一次。

| `SynthDiagnosis` 的字段 | 内容 |
|---|---|
| `cause` | 一句话的原因 |
| `explanation` | 说明 |
| `remedies` | 一条或多条解决办法 |
| `failures` | 该原因解释了哪些现场（下标） |

诊断时提供的上下文：本地设置中的两个合成工具、渲染方式（经典、多线程、实时）与全部现场。

- 诊断器注册在 `kit::SynthDiagnoserRegistry` 中，宿主 `Editor` 以 `synthDiagnosers()` 提供，写法与频率表格式（`FrequencyFormatRegistry`）相同：插件在 `initialize()` 中创建注册对象，在 `aboutToShutdown()` 中销毁。
- 诊断器带优先级，数值大者先运行。针对某个合成工具的诊断器优先于通用的诊断器。一个现场被某个诊断解释之后，兜底的通用说明不再给出。
- 诊断在界面线程上运行，读取的只是记录下来的现场与少量文件（例如合成工具旁的配置文件），不运行任何程序。

## 显示

`DiagnosticBox` 在原有的逐条错误之上增加「可能的原因」一节，每个诊断显示原因、说明与解决办法，原有的逐条错误留在下面供参考。整轨已经写出但有可疑分片时，同样显示诊断。以后的右下角通知窗复用同一组结果。

## 首批诊断器

通用诊断器内置于 `HelloKitSynth`，由宿主注册：

- 路径无法以系统的 ANSI 代码页表示。以窄字符 `main` 读取参数的合成工具得到的是按代码页转换后的参数，无法表示的字符变为问号，因此找不到文件。以 `TextCodec::canEncode()` 检查每个路径参数。
- 程序未能启动。
- Windows 的 NTSTATUS 退出码：`0xC0000135`（找不到依赖的 DLL）、`0xC000007B`（映像格式错误，通常为 32 位与 64 位混用）、`0xC0000005`（访问违规，即合成工具崩溃）。
- 超时。
- 退出码为 0 但没有输出文件，或输出不是 RIFF/WAVE。这是兜底的诊断。
- 路径超过 260 个字符。

moresampler 的诊断器由 FrequencyEditor 插件注册（作者 2026-10-11 决定），该插件已经注册了 moresampler 的频率表格式：

- **重采样器为 moresampler，`resampler-compatibility off`，而拼接不由 moresampler 完成。** 依据见下文实测。解决办法：把 moresampler 目录中 `moreconfig.txt` 的 `resampler-compatibility` 改为 `on`，或把合成器也设为 moresampler。实时模式以内置的拼接读取分片，无论合成器是什么都读不了这样的分片，因此只能改为 `on`。
- **合成器为 moresampler，而重采样器不是 moresampler。** moresampler 作为合成器只读取 moresampler 生成的 `.llsm`（教程「兼容性」一节：「作为工具1时，Moresampler不能与其他的重采样引擎一起使用，因为输入Moresampler的只能是LLSM文件而不能用其他重采样引擎所输出的波形文件」）。
- **`.llsm` 文件的版本比 moresampler 新且不兼容。** 教程「兼容性」一节记载 moresampler 此时停止工作并报错。识别这种情况需要该错误的输出原文，尚未取得，实现前须实测。

## 实测

**moresampler 0.8.4 的 `resampler-compatibility`**（2026-10-11，`.cache/claude/tools/work/moresampler_probe.py`）。复制两份完整的程序目录，分别把 `moreconfig.txt` 的 `resampler-compatibility` 设为 `off`（发布时的默认值）与 `on`，以同一条重采样参数处理同一个样本，工作目录设在程序目录之外：

- `off`：退出码 0。输出路径上的文件为 180 字节的文本，全文为「This is a dummy file created just to fool UTAU's cache mechanism and you really shouldn't expect to see anything here. The vocals are stored in llsm files, if nothing went wrong.」，旁边另写出 `<输出>.llsm.tmp`（开头为 `\x04data`）。
- `on`：退出码 0。输出为 RIFF/WAVE（44100 Hz，16 位，单声道），旁边同样写出 `<输出>.llsm.tmp`。
- 改在程序目录中的设置生效，而工作目录不是程序目录，因此 moresampler 读取的是程序旁的 `moreconfig.txt`。教程「配置」一节记载音源目录中的 `moreconfig.txt` 只支持分析类与合成类的选项，`resampler-compatibility` 属于输出类，因此只看程序旁的文件。

因此该诊断器以分片的开头识别这种情况，不依赖程序的文件名。

出处：moresampler 0.8.4 附带的 `how-to-use.txt`（「By default Moresampler uses LLSM model file as temporary format instead of wav file, so it won't work with other wavtools」），以及《完整Moresampler教程 2016》的「配置」「音源库配置文件」「兼容性」各节。

## 未核实

以下猜测没有依据，在实测证实之前不写入诊断器：某些重采样器只接受 16 位单声道的 wav；某些重采样器必须有 `.frq` 文件。
