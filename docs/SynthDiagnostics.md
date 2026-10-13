# 渲染失败的诊断

本文规定渲染失败时的诊断：在报告「这个音符无法渲染」之外，给出可能的原因与解决办法。作者 2026-10-11 提出要求，诊断器可由插件注册。渲染本身的结构见 [`Synth.md`](Synth.md)，插件的注册方式见 [`Plugins.md`](Plugins.md)。

## 动机

渲染失败时，用户现在看到的是合成工具的输出原文，或者「重采样器没有输出」。原因往往不在音符，而在合成工具的配置、文件路径或系统环境，新手无从判断。诊断器根据失败时的现场判断原因，并给出可以照做的解决办法。

## 失败现场

诊断需要的信息在渲染过程中产生，现在只留下一句消息与音符编号。渲染器为每个观察到的失败记录一个 `SynthFailure`：

| 字段 | 内容 |
|---|---|
| `stage` | 重采样器没有写出分片（`Resampler`），合成器未能启动、对某个音符失败或没有写出整轨（`Wavtool`），实时模式无法读取分片（`Fragment`），经典模式没有写出整轨时已存在的分片（`Unconfirmed`） |
| `noteIndex` | 音符编号，整轨的失败时为空 |
| `program`、`arguments` | 运行的程序与参数 |
| `run` | `SynthToolRun`：是否启动、退出码、是否超时、输出文本，未知时为空 |
| `file` | 诊断要查看的文件：该音符的分片，整轨的失败时为整轨文件 |
| `fileSize`、`fileHead` | 该文件的大小与开头的字节，文件不存在时大小为 -1 |

**以观察到的结果判断失败，不判断分片的格式**（作者 2026-10-11 决定）。重采样器写出的分片可能是只有配套的合成器能读取的中间格式，moresampler 的 LLSM 即是一例，将来的其他合成工具也可能如此。渲染器无法确定哪个合成器能读取哪种格式，因此不以「分片是不是 wav」判断失败。成功的渲染不读取任何分片，也不运行诊断器。

三种渲染器的记录（`SynthOutcome::failures`，实时模式为 `RealtimeSynth::takeFailures()`）：

- 多线程模式记录重采样器没有写出的分片，带有该次调用的完整结果。因 `stopOnFirstFailure` 而没有运行的音符不记录，否则会被误诊为程序未能启动。合成器的调用在两个条件同时成立时记为该音符的失败：退出码非零，并且这次调用没有追加任何数据（`.dat` 与整轨都没有变大）。只看退出码会误报，因为合成工具的退出码不可靠（见 `SynthToolRun::exitCode`）。合成器未能启动，或者整轨没有写出，记为整轨的失败。没有任何一次合成器调用时，整轨缺失的原因已由各次重采样的现场说明，不另记。
- 经典模式由批处理运行合成工具，取不到单次调用的结果，现场的 `run` 为空，只记录缺失的分片与缺失的整轨。批处理本身未能启动时，现场的程序是批处理文件而不是合成器。批处理运行后没有写出整轨时，合成器拒绝了哪些分片无法确定，因此另把每个已存在的分片记为 `Unconfirmed`，读取其开头（作者 2026-10-11 决定）。这一步只在失败之后进行，符合「成功的渲染不读取分片」。内置的通用诊断器不处理这一阶段，以免对每个音符各报一次失败，只有能识别分片内容的诊断器（例如 moresampler 的）认领其中的分片。**局限：** 合成器对某些音符失败、而其他音符（例如休止符）照常写出数据时，整轨存在，这些音符的失败观察不到。此时渲染被视为成功，不运行诊断器，卷帘也不标红，播放出的是这些音符位置上的静音。2026-10-11 以作者的工程实测：moresampler 的 `resampler-compatibility` 为 `off`、合成器为 UTAU 的 `wavtool.exe`，经典模式写出 1 MB 的整轨并报告成功。可行的办法是比较整轨的长度与计划的长度，明显较短即说明有合成器调用没有追加数据，再按「没有写出整轨」的方式读取分片的开头交给诊断器，阈值须先实测 `wavtool.exe` 的追加行为。作者认为现状已经足够（2026-10-11），此项列为优先级极低的未来工作。
- 实时模式以 `WavtoolMixer` 读取每个分片，无法读取的分片无论合成器是什么都记录，因为这一步就是拼接本身。

## 诊断器

`kit::SynthDiagnoser`（`HelloKitSynth`）读取一次渲染的全部现场，返回零个或多个 `SynthDiagnosis`。一次读取全部现场，是因为同一原因常使整轨失败，应报告一次而不是每个音符各报一次。

| `SynthDiagnosis` 的字段 | 内容 |
|---|---|
| `cause` | 一句话的原因 |
| `explanation` | 说明 |
| `remedies` | 一条或多条解决办法 |
| `failures` | 该原因解释了哪些现场（下标） |

诊断时提供的上下文：本地设置中的两个合成工具、渲染方式（经典、多线程、实时）与全部现场。

**运行模式**（作者 2026-10-13 决定）：上下文的 `operation` 区分渲染（`Render`）与频率表的生成（`FrequencyMapGeneration`），生成时另带格式的显示名称（`frequencyFormat`）。诊断器据此决定适用的原因与措辞：渲染的解决办法说「这些音符的样本」「音源、工程或缓存文件夹」，生成的说「这些音频文件」「音源」。未采用的做法是让诊断器从现场的参数（C4、50 毫秒）推断运行模式，因为这依赖参数的具体取值，措辞也仍然需要知道模式。生成时，`kit::FrequencyMapGenerator` 记录每个没有得到频率表的重采样器调用：退出码为 0 的记为 `SynthFailure::FrequencyMap`，由兜底的诊断器报告「重采样器没有写出格式 X 的频率表」，并提示检查重采样器写出的格式（格式按重采样器的文件名选定，改过名的重采样器会对应到别的格式）；其余照常记为 `SynthFailure::Resampler`，由启动失败、退出状态、超时等诊断器解释。音源窗口以「%n 个音频文件没有频率表」为概括显示这些诊断，各文件的消息进入详细信息，重采样器的输出不进入消息框，只写入日志。

- 诊断器注册在 `kit::SynthDiagnoserRegistry` 中，宿主 `Editor` 以 `synthDiagnosers()` 提供，写法与频率表格式（`FrequencyFormatRegistry`）相同：插件在 `initialize()` 中创建注册对象，在 `aboutToShutdown()` 中销毁。
- 诊断器带优先级，数值大者先运行。针对某个合成工具的诊断器优先于通用的诊断器。一个现场被某个诊断解释之后，兜底的通用说明不再给出。
- 诊断在界面线程上运行，读取的只是记录下来的现场与少量文件（例如合成工具旁的配置文件），不运行任何程序。

## 显示

`DiagnosticBox` 在原有的逐条错误之上增加「可能的原因」一节，以列表列出各个诊断，每项显示原因、说明与解决办法。列表的样式由调用方选择，默认为无序列表，渲染失败的诊断使用有序列表（作者 2026-10-11 决定）。原有的逐条错误留在详细信息中供参考。「可能的原因」之前先以一句话说明发生了什么（作者 2026-10-11 要求）：渲染失败时为「无法渲染这些音符」，整轨写出而有音符失败时为「部分音符无法渲染」，实时模式为「若干音符无法渲染，已作静音处理」，各音符的消息进入详细信息。实时模式的诊断在试听停止或暂停时显示（暂停时也显示，作者 2026-10-11 要求），不在播放中途打断，每次失败只报告一次。只有用户以暂停、停止、重新播放或移动播放头结束试听，或试听播放到结尾时，才显示诊断。其他原因导致的停止，例如修改设置、修改合成工具、清除渲染缓存，都丢弃这些失败，不显示诊断（作者 2026-10-13 要求做成通用的规则，取代原先只针对清除缓存的处理，并决定重新播放与移动播放头也报告，因为二者同样由用户触发）。`Playback::playedToEnd()` 区分播放到结尾与调用 `stop()`，`AudioOutput::finished()` 因此只在声源播放完时发出，`stop()` 与 `start()` 替换声源时不再发出。清除渲染缓存时，即使试听未在进行，也丢弃这些失败，因为它们涉及的分片已被删除。

预渲染方式中，诊断器解释了的音符失败在卷帘中显示为失败（红色），即使其分片存在（作者 2026-10-11 决定）。`Playback` 记录这些失败的分片路径，扫描缓存时据此标记。一次预渲染结束时只更新它所渲染的音符的标记，其他音符的标记保留，因此换一个选区渲染不会使之前失败的音符变回正常。缓存文件名不随合成工具的配置改变，修改配置并清除缓存后重新渲染，同名的分片可能已经正常，所以重新渲染某个音符时总是先去掉它的旧标记。实时模式的音符状态由 `RealtimeSynth` 给出，无法读取的分片本来就显示为失败。整轨已经写出、但有音符的合成器调用失败时，同样显示诊断。以后的右下角通知窗复用同一组结果。

## 首批诊断器

通用诊断器内置于 `HelloKitSynth`，由宿主注册：

- 路径无法以系统的 ANSI 代码页表示。以窄字符 `main` 读取参数的合成工具得到的是按代码页转换后的参数，无法表示的字符被替换为替代字符，因此无法打开文件。以 `TextCodec::canEncode()` 检查每个路径参数。
- 程序未能启动。
- Windows 的 NTSTATUS 退出码：`0xC0000135`（找不到依赖的 DLL）、`0xC000007B`（映像格式错误，通常为 32 位与 64 位混用）、`0xC0000005`（访问违规，即合成工具崩溃）。
- 超时。
- 其余的失败，按阶段说明：重采样器没有写出分片，合成器对音符失败（分片不是 wav 时附上其开头），合成器没有写出整轨，实时模式无法读取分片。这是兜底的诊断。
- 路径超过 260 个字符。

moresampler 的诊断器由 FrequencyEditor 插件注册（作者 2026-10-11 决定），该插件已经注册了 moresampler 的频率表格式：

- **重采样器为 moresampler，`moreconfig.txt` 中 `resampler-compatibility off`，而拼接不由 moresampler 完成**（`MoresamplerDiagnoser`）。识别方式：合成器对某个音符失败（多线程模式），实时模式无法读取分片，或经典模式没有写出整轨，而该音符的分片以 moresampler 的那段占位文字开头。依据见下文实测。解决办法：把 moresampler 目录中 `moreconfig.txt` 的 `resampler-compatibility` 改为 `on`，或把合成器也设为 moresampler，修改后清除渲染缓存，因为已写出的分片仍是占位文件。实时模式以内置的拼接读取分片，无论合成器是什么都无法读取这样的分片，因此不提供改合成器的办法。若 `moreconfig.txt` 已为 `on` 或不存在，moresampler 写出的是 wav，这些占位分片来自修改之前的渲染，此时只提供清除缓存的办法。
- **`.llsm` 文件的版本比 moresampler 新且不兼容。** 教程「兼容性」一节记载 moresampler 此时停止工作并报错。识别这种情况需要该错误的输出原文，尚未取得，实现前须实测。

教程「兼容性」一节还记载 moresampler 作为合成器（工具 1）时不能搭配其他重采样器，因为它只读取 LLSM 文件。这一点在 0.8.4 上不成立，见下文实测，因此不设这个诊断器。

## 实测

**moresampler 0.8.4 的 `resampler-compatibility`**（2026-10-11，`.cache/claude/tools/work/moresampler_probe.py`）。复制两份完整的程序目录，分别把 `moreconfig.txt` 的 `resampler-compatibility` 设为 `off`（发布时的默认值）与 `on`，以同一条重采样参数处理同一个样本，工作目录设在程序目录之外：

- `off`：退出码 0。输出路径上的文件为 180 字节的文本，全文为「This is a dummy file created just to fool UTAU's cache mechanism and you really shouldn't expect to see anything here. The vocals are stored in llsm files, if nothing went wrong.」，旁边另写出 `<输出>.llsm.tmp`（开头为 `\x04data`）。
- `on`：退出码 0。输出为 RIFF/WAVE（44100 Hz，16 位，单声道），旁边同样写出 `<输出>.llsm.tmp`。
- 改在程序目录中的设置生效，而工作目录不是程序目录。探针以完整路径作为 `argv[0]`，moresampler 读取的是该路径旁的 `moreconfig.txt`，见下文「`argv[0]`」。教程「配置」一节记载音源目录中的 `moreconfig.txt` 只支持分析类与合成类的选项，`resampler-compatibility` 属于输出类，因此只看程序旁的文件。

**moresampler 0.8.4 旁边没有 `moreconfig.txt`**（2026-10-11，`.cache/claude/tools/work/moresampler_noconfig_probe.py`）。moresampler 输出「load_config: configuration file not found.」，写出的是 RIFF/WAVE，旁边同样写出 `.llsm.tmp`。因此写出占位文字的条件是 `moreconfig.txt` 存在且 `resampler-compatibility off`。

**moresampler 0.8.4 按 `argv[0]` 查找 `moreconfig.txt`**（2026-10-11，`.cache/claude/tools/work/moresampler_argv0_probe.py`）。同一份 `off` 的程序目录，执行的文件相同，工作目录在程序目录之外：`argv[0]` 为完整路径时写出占位文字，只有文件名时输出「load_config: configuration file not found.」并写出 RIFF/WAVE。`argv[0]` 须为完整路径，与 UTAU 相同：只传文件名时，moresampler 在多线程与实时模式下忽略 `moreconfig.txt` 的全部设置，而经典模式的 `temp.bat` 传完整路径，三种模式的结果因此不同（`three_modes_probe.py`，作者的工程，562 个音符）。传完整路径时三种模式写出的都是占位文字。

**moresampler 0.8.4 作为合成器拼接其他重采样器写出的 wav**（2026-10-11，`.cache/claude/tools/work/moresampler_wavtool_probe.py`）。以 UTAU 的参数格式拼接一个没有 `.llsm.tmp` 的 wav 分片：退出码 0，输出「data index appended」。末尾带 `LAST_NOTE` 时写出 RIFF/WAVE 的音轨，样本峰值 17013，与分片的 17014 一致，说明分片被正常拼接。

**UTAU 的 `wavtool.exe` 读取上述分片**（2026-10-11，`.cache/claude/tools/work/wavtool_dummy_probe.py`，UTAU 附带的 `wavtool.exe`，2010-02-19）。以 UTAU 的参数格式拼接一个分片：

- `off` 写出的文本分片：输出「not a wave file」，退出码为 -1（`0xFFFFFFFF`），不写出 `.whd` 与 `.dat`。
- `on` 写出的 wav 分片：退出码 0，`.dat` 中追加 22050 个样本。

因此 moresampler 搭配 UTAU 的合成器时，失败表现为合成器对这些音符报错且不追加数据，可以直接观察到。诊断器再以分片的开头确认原因，不依赖程序的文件名。

出处：moresampler 0.8.4 附带的 `how-to-use.txt`（「By default Moresampler uses LLSM model file as temporary format instead of wav file, so it won't work with other wavtools」），以及《完整Moresampler教程 2016》的「配置」「音源库配置文件」「兼容性」各节。

## 未核实

以下猜测没有依据，在实测证实之前不写入诊断器：某些重采样器只接受 16 位单声道的 wav，某些重采样器必须有 `.frq` 文件。
