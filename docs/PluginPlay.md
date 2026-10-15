# plugin-play：原版插件的试听

本文档是 plugin-play 的设计，写于 2026-10-14，已实现，作者已以 PitTrace 验收（2026-10-14）。plugin-play 是 playinplugin 的兼容程序，供 PitTrace 等原版插件在 HelloUtau 中试听。原版的行为见 [`claude/playinplugin.md`](claude/playinplugin.md)，原版插件的支持见 [`ClassicPluginHost.md`](ClassicPluginHost.md)。

## 动机

PitTrace（拡張ピッチエディタ）等插件的「試聴」按钮调用插件文件夹中的 `playinplugin.exe`。原版 playinplugin 自行生成批处理并调用合成工具，在 HelloUtau 中使用它有三个问题：

- 合成工具取自磁盘上的工程文件（由 `CacheDir` 推出路径），HelloUtau 的工程是 `.usth`，该路径不存在时退回 UTAU 的 `setting.ini`，与 HelloUtau 的设置无关。
- 它执行工程文件中写的合成工具，不经任何信任检查，违反 CLAUDE.md「安全底线」。
- 它不采用工程的全局 Flags，试听与正式渲染不一致。

plugin-play 以同样的命令行接受调用，把请求交给运行该插件的 HelloUtau，由 HelloUtau 以自己的设置渲染并播放。

## 作者的决定（2026-10-14）

- 名称为 plugin-play。程序照 `hello-updater`（`hellokit/tools/updater`）的做法：只依赖标准库与系统接口，静态链接 C++ 运行库，不链接 Qt，也不链接 stdcorelib。
- 宿主弹出临时的试听对话框，取消则不播放。配置与宿主相同，使用宿主设置的音频设备。
- 采用工程的全局 Flags，与原版不同。
- 工程的合成工具不可信时显示错误窗口，不渲染，不改用其他合成工具。
- 底稿是宿主本次运行插件时的工程快照与选区，输入文件只用于核对。
- `/noplay` 时不弹出试听对话框。
- 不清空输入文件，与原版不同。
- 先只支持 Windows。
- 部署须由用户在设置页确认，届时把 `plugin.txt` 转写为 `plugin.json` 并请用户确认编码，替换的记录写入 `plugin.json`。
- 没有宿主时运行原版（`playinplugin-original.exe`），没有原版才报错。
- 渲染采用预渲染：渲染选区后播放，不采用实时模式。
- 试听对话框存在期间 plugin-play 的进程保持运行，因为插件可能以此判断试听是否结束。

## 组成

| 部分 | 位置 | 职责 |
|---|---|---|
| `hello-plugin-play` | `helloutau/tools/plugin-play`（2026-10-15 由 `hellokit/tools` 移入，作者决定，因为它只服务于 ClassicPluginHost 插件；测试在 `helloutau/tests/auto/tools/plugin-play`） | 解析命令行，经命名管道把请求交给宿主，等待回复后退出 |
| 管道服务器 | ClassicPluginHost | 插件运行期间接受请求，核对、叠加修改、渲染，回复结果 |
| 试听对话框 | ClassicPluginHost（`Internal`） | 显示渲染进度，播放，取消 |

部署时 `hello-plugin-play.exe` 以 `playinplugin.exe` 的名称复制到插件文件夹，因为 PitTrace 在自己的文件夹中按这个名称查找。

## 命令行

与原版的实测一致（[`claude/playinplugin.md`](claude/playinplugin.md)「命令行」）：

```
playinplugin.exe <输入文件> [/m:<修改文件>] [/o:<输出文件>] [/noplay] [/nodeletewav] [/nodeletecache] [/debug]
```

- 第一个参数是输入文件。其余参数中，`/m:`、`/o:` 以前缀识别，四个开关须完全一致，区分大小写。未知的参数被忽略。
- 相对路径按工作目录解析为绝对路径后再交给宿主。
- 退出码恒为 0，与原版的实测一致。没有管道时例外，见「没有宿主时的行为」。
- 唯一的参数为 `/?`、`-h` 或 `--help` 时显示用法，为 `--version` 时显示版本（`hellokit/Config.h` 的 `HELLOKIT_VERSION`），退出码为 0（作者 2026-10-15 要求）。只在唯一参数时识别，因为原版把第一个参数一律当作输入文件，而 `/?` 不可能是文件名。标准输出被重定向到文件或管道时写到标准输出，否则以消息框显示，因为它是没有控制台的窗口程序。

## 管道

- **作业对象**：宿主把每次运行的插件进程放入一个作业对象（`ClassicPluginRunner`，供「取消」结束整棵进程树）。`CreateProcess` 以挂起状态启动插件，加入作业后再恢复，插件此后启动的进程都自动属于这一作业。作业没有 `JOB_OBJECT_LIMIT_BREAKAWAY_OK`，插件以 `CREATE_BREAKAWAY_FROM_JOB` 创建的进程因此无法脱离作业。作业只包含本次运行的进程，通常是插件与 plugin-play 两个。
- **管道名**：宿主在插件启动后创建一个 `QLocalServer`，名称为 `helloutau-plugin-play-<插件进程的进程号>`，选项为 `QLocalServer::UserAccessOption`。
- **查找宿主**：plugin-play 以 `QueryInformationJobObject(nullptr, JobObjectBasicProcessIdList, ...)` 取得自身所在作业的进程号（句柄为空即调用进程所在的作业），对其中自身以外的进程号连接 `\\.\pipe\helloutau-plugin-play-<进程号>`，存在的管道即本次运行的宿主。这一方式不依赖插件传递环境变量或参数，中间进程退出也不影响作业的归属，并且只查找本次运行的进程，不涉及其他 HelloUtau 实例。
- **找不到宿主的情形**：plugin-play 不在作业中（`IsProcessInJob` 不成立），或作业中没有对应的管道。例如由官方 UTAU 运行插件，或 `shell=use` 的插件交给已在运行的程序处理、宿主无法取得进程句柄时。处理见「没有宿主时的行为」。
- **消息**：每条消息是一行 UTF-8 的 JSON，以 LF 结尾。plugin-play 用 `WideCharToMultiByte` 转换路径并手工拼写 JSON，宿主用 `QJsonDocument` 解析。
- **请求**：

  ```json
  {"input": "C:\\...\\Base.ust", "modify": "C:\\...\\testplay.txt", "output": null,
   "play": true, "keepOutput": false}
  ```

  `modify`、`output` 未给出时为 `null`。`/nodeletecache` 与 `/debug` 不传递，因为缓存与批处理由宿主管理。
- **回复**：`{"result": "done"}`、`{"result": "canceled"}` 或 `{"result": "failed", "message": "..."}`。
- **进程的存续**：宿主在试听对话框关闭后才回复（`play` 为假时在渲染结束后回复），plugin-play 在收到回复或管道断开后才退出，不设超时。试听对话框存在期间 plugin-play 因此一直在运行，插件可以据此判断试听是否结束。反过来，plugin-play 的进程被插件结束、管道断开时，宿主停止渲染与播放并关闭试听对话框。
- **前台窗口**：Windows 只允许前台进程设置前台窗口。plugin-play 由前台的插件启动，连上管道后以 `GetNamedPipeServerProcessId` 取得宿主的进程号并调用 `AllowSetForegroundWindow`，宿主因此能把试听对话框与错误窗口提到最前，而不只在任务栏闪烁。宿主收到请求时记下当时的前台窗口（通常是插件的窗口），这些窗口关闭后把前台还给它（作者 2026-10-14 同意）。
- 管道服务器只在插件运行期间存在，插件结束或被取消时关闭。同一时间只处理一个请求，前一个请求未回复时，后到的请求立即得到 `failed`。

## 没有宿主时的行为

找不到宿主时（见「管道」），plugin-play 按以下顺序处理：

- 同一文件夹中有 `playinplugin-original.exe` 时，以相同的参数运行它，等待它结束，并以它的退出码结束。原版在部署时以这个名称保留，见「部署」。
- 没有原版时，以 `MessageBoxW` 说明它须由 HelloUtau 运行插件时调用，然后以退出码 1 结束。

## 宿主的处理

1. **核对**：以 `utau::PluginInput` 按插件的编码读取输入文件，其音符数与选区起点须与本次运行写出的临时文件一致，否则回复 `failed` 并显示错误窗口。PitTrace 的输入文件是临时文件的原样副本，因此一致。
2. **叠加修改**：给出 `modify` 时由 `ClassicPluginExchange::preview()` 叠加到本次运行的工程快照的副本。它以快照建立一个临时的 `kit::ProjectSession`，调用正式应用插件结果的同一个 `apply()`（按段落顺序，省略的条目不变，空值恢复默认），返回修改后的工程与移动后的选区，文档不受影响。文件不存在或不含音符段落时按没有修改处理，与原版一致。叠加的结果不写回输入文件。
3. **合成工具**：使用工程的 `Tool1`、`Tool2`。`SynthToolTrust::isAllowed()` 不成立时显示错误窗口，说明须先在正式渲染或工程属性中信任这些合成工具，回复 `failed`，不询问，不渲染。
4. **渲染**：采用预渲染，即 `Playback::play()` 的方式，渲染完成后再播放，不采用实时模式。运行器（批处理或多线程）与宿主的渲染设置相同，缓存在本次插件运行的临时目录中，见「宿主一侧的接口」。选区插件渲染副本中选区对应的音符，插件插入或删除音符使选区的长度改变时，渲染的范围随之改变。整轨插件渲染整个音轨。全局 Flags 等一切设置与正式渲染相同。
5. **播放**：`play` 为真时在试听对话框中播放。`play` 为假时不弹出对话框，只渲染，出错时显示错误窗口。
6. **输出文件**：给出 `output` 时把渲染结果写到该路径，覆盖已有的文件，回复之后按 `keepOutput` 删除或保留，与原版一致。没有 `output` 时只在宿主的临时目录中渲染，不在插件文件夹中留下文件。
7. **回复**：试听对话框关闭或渲染结束后回复。

### 原版行为的对应

| 原版 | plugin-play |
|---|---|
| 合成工具取自 `CacheDir` 推出的工程文件或 `setting.ini` | 工程的 `Tool1`、`Tool2`，须可信 |
| 不采用工程的全局 Flags | 采用 |
| 叠加结果写回输入文件，默认结束后清空输入文件 | 输入文件不被修改 |
| 在输入文件的文件夹中生成 `temp.bat`、缓存与 `plugin_temp.wav` | 只在宿主的临时目录中渲染，`/o:` 例外 |
| `/nodeletecache`、`/debug` | 接受，不起作用 |
| 播放结束后退出 | 试听对话框关闭后退出 |

### 已按约定确定的做法（作者可推翻）

- 试听对话框在渲染时显示进度与「取消」，渲染完成后自动播放，提供「停止」「重播」与「关闭」。播放结束后对话框保持打开，关闭后才回复。
- 试听沿用项目窗口的播放头。播放头按文档的时间轴显示，插件插入或删除音符时与音符对不上，播放仍按渲染出的音频结束（作者 2026-10-14 决定保持）。
- 试听对话框是插件等待对话框（`ClassicPluginRun.cpp` 的 `waitFor()`）的子窗口。等待对话框以 `exec()` 运行事件循环，管道服务器与试听对话框因此不需要另开线程。

## 宿主一侧的接口

- 渲染入口：`Playback::play()` 增加一个重载，渲染调用方给出的 `Project` 值，音源与工程目录取自文档，缓存目录由调用方给出，插件不构造文档（作者 2026-10-14 决定）。试听使用项目窗口的 `Playback`，由 `ProjectWindow::playback()` 取得：它已按设置配置了运行器与临时目录，开始渲染前会停止窗口的后台渲染，状态栏的进度与失败时的诊断框也随之适用于试听。
- ClassicPluginHost 须链接 Qt Network（`QLocalServer`）。
- 渲染缓存：试听不使用工程的渲染缓存。每次插件运行在项目窗口的临时目录中另建一个临时目录作为试听的缓存，插件运行结束时删除（作者 2026-10-14 决定）。同一次插件运行中的多次试听共用这个目录，未改动的音符因此不再重新合成。工程的缓存以音符位置命名片段并清理被取代的片段，插件的插入与删除会改变其后音符的位置，共用缓存会误删工程的片段。`Playback` 因此提供 `temporaryDirectory()`，`play()` 的重载接受缓存目录。

## 部署

- 设置的「Classic Plugins」页有「试听程序」一组，列出发现的插件，每行有安装、更新与卸载，均须用户确认。界面中文统一称「卸载」（作者 2026-10-15 定），与「一键卸载」一致。文件的操作由 `ClassicPluginPreviewInstallation` 完成。
- 程序为 `HelloUtau.exe` 旁的 `hello-plugin-play.exe`（安装规则与 `hello-updater` 相同，位于 `bin`），复制为插件文件夹中的 `playinplugin.exe`。原有的 `playinplugin.exe` 改名为 `playinplugin-original.exe` 保留，没有宿主时由 plugin-play 运行。
- 没有 `plugin.json` 的插件，安装前把 `plugin.txt` 转写为 `plugin.json`（`name`、`execute`、`shell`、`notes`、`ustversion`，不写 `charset`，临时文件因此仍为本地编码），因为有 `plugin.json` 时不再读取 `plugin.txt`（[`note.md`](note.md)「插件」）。`plugin.txt` 的编码由用户在与打开 UST 时相同形式的对话框中选择：候选编码、无效字节序列的灰显与预选，以及按所选编码解码的全文（作者 2026-10-14 要求）。
- 记录写入 `plugin.json` 的 `hostData/ClassicPluginHost/playinplugin`（note.md「插件」）。卸载时还原原有程序，并只删去这条记录。`plugin.json` 一律保留，包括转写来的，以免丢失安装后用户或其他原生插件的修改。原先的做法是删除转写来的 `plugin.json`，作者 2026-10-15 指出会丢失这些修改，改为保留。
- 安装中途失败时撤回已做的步骤。
- 插件文件夹中的程序与当前的 `hello-plugin-play.exe` 内容不同时显示为旧版本，可以更新。
- 「一键安装」只处理推荐的插件（`ClassicPluginPreviewInstallation::isRecommended()`），即本来就带 `playinplugin.exe`、以它试听的插件（作者 2026-10-15 要求）：
  - 未安装且文件夹中有 `playinplugin.exe` 的，安装；
  - 已安装旧版本且安装时保留了原有程序的，更新。
  - 需要转写的插件逐个询问编码，取消则跳过该插件。
- 插件列表上方注明推荐的插件个数，以及「一键安装」只处理这些插件。推荐的插件的状态以粗体标为「推荐安装」或「推荐更新」。「一键卸载」确认一次后卸载全部（作者 2026-10-14 要求）。其他插件只能逐个安装，以免为不试听的插件转写 `plugin.json`。

## 实施步骤

1. ~~**叠加修改**~~：`ClassicPluginExchange::preview()`，以临时会话复用 `apply()`，无须拆出合并逻辑。QtTest。
2. ~~**`hello-plugin-play`**~~：`helloutau/tools/plugin-play`，命令行解析、按作业查找宿主、管道客户端、运行原版与没有原版时的消息框，CMake 照 `hello-updater`，只在 Windows 上构建。测试 `helloutau/tests/auto/tools/plugin-play` 用 Python 运行程序：测试进程创建作业，在其中启动充当插件的进程，以 CPython 的 `_winapi` 创建以该进程命名的管道充当宿主，检查请求的内容、等待回复、管道关闭后退出，以及作业中没有宿主时以原样的参数运行原版（`plugin-play-original-stub` 充当原版）并返回其退出码。
3. ~~**管道服务器与渲染入口**~~：`Playback` 的重载与 `ProjectWindow::playback()`。`ClassicPluginRunner::processId()` 提供插件进程的进程号，`ClassicPluginPreviewServer` 以它命名管道，每个连接读取一行请求，核对输入文件的音符数与选区起点，以 `ClassicPluginExchange::preview()` 叠加修改后发出 `requested()`，由接收方渲染并调用 `reply()`，连接在回复前关闭时发出 `abandoned()`。QtTest 以 `QLocalSocket` 模拟 plugin-play。
4. ~~**试听对话框**~~：`Internal/ClassicPluginPreview`，在插件运行期间（`ClassicPluginRun.cpp` 的等待对话框之内）建立管道服务器与试听缓存，插件结束、应用结果之前停止渲染并删除缓存。合成工具不存在或不可信时显示错误窗口。`/noplay` 以 `Playback::renderTrack()` 的重载只渲染。其余请求显示试听对话框，渲染完成后自动播放，`/o:` 的文件为渲染结果的副本。作者已试用。
5. ~~**部署**~~：见上节。`plugin.json` 的 `ustversion` 与 `hostData` 键（2026-10-15 由根上的 `ClassicPluginHost` 键移入 `hostData`），安装类 `ClassicPluginPreviewInstallation`（QtTest），设置页（作者已试用）。
6. ~~**验收**~~：作者以 PitTrace 试听，2026-10-14 通过。

## 不做的事

- macOS 与 Linux：插件在 Wine 中运行，Win32 命名管道无法连接宿主的 Unix 域套接字，待 ClassicPluginHost 支持 Wine 时再定。
