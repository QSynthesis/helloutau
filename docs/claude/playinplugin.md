# playinplugin 与 PitTrace 的调用

本文档记录 playinplugin（为原版插件提供试听的辅助程序）的来源、PitTrace 调用它的方式，以及原版命令行与渲染行为的黑盒实测结果，供 HelloUtau 提供兼容的试听程序时参照。设计尚在讨论中，见文末。

## 来源

- [UTAU ユーザー互助会@ウィキ 的插件列表](https://w.atwiki.jp/utaou/pages/36.html)：「playinplugin｜プラグインに試聴機能を追加する（プラグイン側の対応が必要）｜2012/06/13｜masao」。下载链接 <http://www.mediafire.com/?ccfpl5pq2yubp> 是账号 `n_masao` 的公开文件夹「playinplugin」（2012-06-08 建立），其中有 `playinplugin20120609.zip`、`playinplugin201206091314.zip` 与 `playinplugin20120613.zip`。2012-06-13 的压缩包只含 `playinplugin.exe`（288,768 字节），没有说明文件。
- Zteer 的拡張ピッチエディタ（PitTrace）2.15 的 `readme.txt`：「試聴ボタンを押すと、入力中のピッチを試聴することができます。試聴には、panda氏が作成された"playinplugin.exe"が必要です」，链接指向上面的列表。作者在两处的记载不一致（masao 与 panda），下载文件夹的所有者是 masao。
- 作者汉化的 PitTrace 2.15（「至精至简」2020 年的 `读我.txt`）注明：试听功能不读取全局参数（用 moresampler 时须对每个音符加 `e` flag），修改合成工具或音源后须先保存工程再运行 PitTrace。下文的实测与这两点一致。

公开资料中没有找到 playinplugin 的命令行说明，下文的参数与行为全部来自黑盒实测。作者以前的重写（`.cache/PlayInPlugin`）的参数与原版不同，不作为依据。

## PitTrace 的调用

2026-10-14 实测：把 PitTrace 2.15 复制到 UTAU 的 `plugins` 下，以记录参数的替身代替 `playinplugin.exe`（源码与结果在 `.cache/utau-probe/playinplugin/`），作者在 PitTrace 中点「試聴」一次。

```
playinplugin.exe "<插件文件夹>\Base.ust" /m:"<插件文件夹>\testplay.txt"
```

- 工作目录为插件文件夹，没有其他参数。两个文件都由 PitTrace 写在自己的文件夹里。
- **`Base.ust` 是 UTAU 交给 PitTrace 的临时文件的原样副本**，即插件的输入。
- **`/m:` 所指的 `testplay.txt` 是 PitTrace 当前的编辑结果**，写法与插件写回 UTAU 的结果文件相同：`[#SETTING]`（只有 `Mode2=True`），每个音符一个编号段落，只写改动的条目（本次为 `PBW`、`PBY`、`PBS`、`PBM`），未改动的音符只有段落头。

## 命令行

2026-10-14 实测原版（与汉化版 PitTrace 附带的 `playinplugin.exe` 哈希相同）。探针与结果在 `.cache/utau-probe/playinplugin/original/`：每个探针在独立的目录副本中运行，目录结构与 UTAU 相同（`UTAU/setting.ini`、`UTAU/plugins/<插件>/playinplugin.exe`），输入取自上一节 PitTrace 交出的两个文件。

```
playinplugin.exe <输入文件> [/m:<修改文件>] [/o:<输出文件>] [/noplay] [/nodeletewav] [/nodeletecache] [/debug]
```

| 参数 | 实测行为 |
|---|---|
| `<输入文件>` | 插件的临时文件，必须是第一个参数。第一个参数为开关时不产生任何文件 |
| `/m:<路径>` | 把该文件中各编号段落的条目叠加到输入文件的同号段落。`Lyric`、`NoteNum`、`Length`、`Velocity`、`Flags`、`PBS`、`PBW`、`PBY`、`Intensity`、`Envelope` 的改动均体现在渲染中。输入文件中没有的段落被忽略。完整格式的 UST 同样接受，文件不存在时按没有修改处理 |
| `/o:<路径>` | 渲染结果写到该路径，代替默认的 `plugin_temp.wav`。已有的文件被覆盖，播放与删除作用于该文件 |
| `/noplay` | 不播放 |
| `/nodeletewav` | 保留渲染结果，也不清空输入文件 |
| `/nodeletecache` | 保留缓存目录 `<输入文件名>.cache` 及其中的 wav |
| `/debug` | 保留 `temp.bat` 与 `temp_helper.bat` |

- 开关区分大小写：`/NOPLAY` 不起作用，程序照常播放。未知的参数被忽略。
- **默认行为即 PitTrace 的调用**：渲染并播放，结束后删除渲染结果、缓存目录与两个批处理文件，并把输入文件清空为 0 字节。播放期间进程不退出（1.6 秒的音频，运行时间由 1.2 秒增至 3.8 秒）。
- **给出 `/m:` 时，叠加的结果写回输入文件**，各段落内的条目按键名排序。文件不存在时同样写回。没有 `/m:` 时输入文件不被改写。

## 渲染

- 生成的文件都位于输入文件所在的文件夹，与工作目录无关。渲染经 `temp.bat` 与 `temp_helper.bat` 调用 resampler 与 wavtool，两个批处理文件的结构与 UTAU 渲染时写出的相同。
- 曲速取自输入文件的 `Tempo`，音源取自输入文件的 `VoiceDir`。先行发声、重叠等由程序按音源的 `oto.ini` 计算，不采用输入文件的 `@preuttr` 等只读条目：`/m:` 改动歌词后，批处理中的值随新的原音设定变化，而写回的 `@preuttr` 仍是旧值。
- **合成工具取自工程文件的 `Tool1`、`Tool2`**，相对路径相对于 UTAU 的文件夹解析。工程文件的路径由输入文件的 `CacheDir` 把扩展名 `.cache` 换成 `.ust` 得到，`Project=` 不被采用：`Project=` 与 `CacheDir` 指向不同的工程时，采用的是 `CacheDir` 对应的工程。没有 `CacheDir` 或工程文件不存在时，合成工具取自 UTAU 文件夹中 `setting.ini` 的 `Tool1Path`、`Tool2Path`。工程文件只被读取，不被修改。
- 工程的全局 Flags 不被采用（工程写有 `Flags=g+7` 时批处理中各音符的 flag 均为空），音符自身的 `Flags` 被采用。
- 以 PitTrace 交出的 106 个音符的完整文件实测，渲染出 40 秒的单声道 44.1 kHz 16 位音频。

## `ustversion=1.00` 的临时文件

PitTrace 的 `plugin.txt` 写有 `ustversion=1.00`，`Base.ust` 因此是 UTAU 按 1.00 写出的临时文件的实例。各格式的对照见 [`utau-plugin-protocol.md`](utau-plugin-protocol.md)「条目格式」。

- 开头没有 `[#VERSION]`，`[#SETTING]` 依次为 `UstVersion=1.00`、`Project`、`Tempo`、`VoiceDir`、`CacheDir`、`Mode2`，路径为绝对路径，本地编码。
- 音符的条目与 1.20 相同的有 `Length`、`Lyric`、`NoteNum`、`PreUtterance=`（空值）、`Intensity`、`Envelope`、`PBW`、`PBS`、`PBY`、`VBR`、`Velocity`、`VoiceOverlap`。调制写作 `Moduration`（不是 `Modulation`）。
- 只读条目为 `@preuttr`、`@overlap`、`@stpoint`、`@filename`、`@alias`，另有 1.20 的样本中没有的 `@cache`（该音符的缓存文件的绝对路径）。
- 本次全选，没有 `[#PREV]` / `[#NEXT]`，也没有 `[#TRACKEND]`。

## 对 HelloUtau 的意义（待讨论）

作者 2026-10-14 的设想：HelloUtau 提供名为 plugin-play 的兼容程序（交给插件时的文件名为 `playinplugin.exe`），不链接 Qt，启动后经命名管道把两个文件交给 HelloUtau，由 HelloUtau 以自己的设置渲染，并显示临时的试听对话框。读取临时文件与结果文件分别对应 stdutau 的 `PluginInput` 与 `PluginResult`，叠加修改与 `ClassicPluginExchange::apply()` 相同，但作用于临时副本，不修改工程。原版从磁盘上的工程文件读取合成工具，因此试听结果取决于工程是否已保存，兼容程序改由宿主提供合成工具后不再有此限制。**兼容程序采用工程的全局 Flags**（作者 2026-10-14 决定），与原版不同，试听结果因此与正式渲染一致。尚待决定的问题见对话记录与 `.cache/claude` 的待办。
