# terminal-stub：在终端窗口中运行渲染脚本

本文档是 macOS 与 Linux 上在终端窗口中运行经典渲染脚本的设计，写于 2026-10-19，尚未实现，「待作者决定」一节的事项尚待确定。渲染脚本本身见 [`Synth.md`](Synth.md)「一、经典（渲染脚本）」。

## 动机

Windows 上的 `temp.bat` 在可见的控制台窗口中运行，与官方 UTAU 相同，用户能直接看到脚本的输出（`SynthToolProcess::runScript()`，`STARTF_USESHOWWINDOW`）。macOS 与 Linux 上的 `temp.sh` 以 `/bin/sh` 在后台运行，输出写到 HelloUtau 自己的标准输出，从 Finder 或桌面启动时无处可见。

控制台窗口在 Windows 上只是子进程的一个属性，进程仍是 HelloUtau 的子进程，等待、返回码、取消与超时都照常可用。macOS 与 Linux 上的终端窗口属于终端程序，脚本必须由终端程序启动，HelloUtau 因此失去对进程的控制：

- `gnome-terminal` 的进程归 `gnome-terminal-server` 所有，命令立即返回。
- `xterm -e` 等待脚本结束，但返回码是 xterm 自己的。
- macOS 的 Terminal 由 AppleScript 或 LaunchServices 启动，不提供进程号与返回码。

## 设计

### 概述

终端中运行的不是渲染脚本，而是辅助程序 `hello-terminal-stub`。它启动 `/bin/sh temp.sh`，经本地 socket 向 HelloUtau 报告脚本的进程号与返回码。HelloUtau 从 socket 得知结果，按进程号取消，不依赖终端程序的返回码与生命周期。这一结构参照 Qt Creator 的外置终端（见「参考」）。

Windows 不使用该程序，现有的控制台窗口不变。

### 组成

- **`hello-terminal-stub`**（`hellokit/tools/terminal-stub`）：照 `hello-fswatcher` 的做法，只依赖 stdcorelib 与系统接口，不链接 Qt，只在 macOS 与 Linux 上构建。与其他辅助程序安装在相同的位置。
- **启动终端的部分**（hellokit，名称待定）：选择终端程序、生成启动参数、创建 socket 并等待报告。`SynthToolProcess::runScript()` 在 macOS 与 Linux 上经由它运行脚本，`SynthToolRun` 的 `started`、`exitCode`、`canceled`、`timedOut` 的含义不变。
- **设置项**：Linux 的终端程序与参数，以及是否在终端中运行，见「待作者决定」。

### 辅助程序的命令行与行为

```text
hello-terminal-stub --socket <path> --token <hex> --cwd <dir> -- /bin/sh <temp.sh>
```

1. 连接 `--socket`，先发送 `--token`，再将 `--cwd` 设为工作目录。
2. 以 `fork` 与 `execv` 启动 `--` 之后的参数向量，子进程以 `setpgid(0, 0)` 成为新进程组的组长，渲染脚本启动的合成工具都在这个组中。
3. 发送子进程的进程号，等待子进程结束，发送返回码或终止信号，然后退出，终端窗口随之关闭。
4. 收到 `SIGHUP`（用户关闭了终端窗口）或 `SIGTERM` 时，先结束子进程组，再退出。
5. 无法连接、无法设定工作目录或无法启动子进程时，在终端中打印原因，经 socket 报告（已连接时），以非零值退出。

### 协议

每条消息为一行 UTF-8 文本，以 `\n` 结束：

| 消息 | 含义 |
|---|---|
| `token <hex>` | 第一条消息，与 HelloUtau 生成的随机值比较，不符即断开 |
| `pid <n>` | 子进程（进程组组长）的进程号 |
| `exit <code>` | 子进程以返回码结束 |
| `signal <n>` | 子进程被信号终止 |
| `error <message>` | 子进程未能启动 |

HelloUtau 的处理：

- 收到 `exit` 时以其为返回码，收到 `signal` 或 `error` 时返回码为 -1，与 `runScript()` 现有的约定一致。
- 取消与超时时向进程组发送 `SIGTERM`，宽限期后发送 `SIGKILL`，与现有的 `killTree()` 相同。
- 未收到 `exit` 而 socket 断开，表示辅助程序异常结束或窗口被关闭，按失败处理并报告。
- 终端程序启动后若干秒内没有连接，报告「终端未能启动辅助程序」，结束等待。

### socket 的位置与权限

socket 放在 HelloUtau 本次运行新建的私有目录中，目录权限为 0700，因为有的系统不检查 socket 文件本身的权限。`sockaddr_un::sun_path` 在 macOS 上只有 104 字节，路径须短，目录名与文件名都用一两个字符。token 防止同一用户的其他进程冒充辅助程序。

不使用管道，因为辅助程序的标准输入输出是终端。不使用文件轮询，因为辅助程序异常结束时 socket 立即断开，轮询文件无法及时发现这种情况。

### Linux 的终端程序

按下表的顺序在 `PATH` 中查找，使用第一个找到的程序。表的内容参照 Qt Creator 的 `knownTerminals`：

| 程序 | 执行参数 | 命令的形式 |
|---|---|---|
| `x-terminal-emulator` | `-e` | 参数向量 |
| `gnome-terminal` | `--` | 参数向量 |
| `konsole` | `--separate -e` | 参数向量 |
| `xfce4-terminal` | `-x` | 参数向量 |
| `xterm` | `-e` | 参数向量 |
| `terminator` | `-e` | 单个字符串 |

- `x-terminal-emulator` 是 Debian 系的 alternatives 链接。它指向 `terminator` 时，改按 `terminator` 的形式传参（Qt Creator 的 QTCREATORBUG-32111）。
- 命令须为单个字符串的终端，以 POSIX 单引号的形式引用每个参数。参数只有辅助程序路径、socket 路径、token、工作目录与脚本路径，都由 HelloUtau 生成，不含工程中的字符串。
- 用户可在设置中指定终端程序与执行参数。找不到任何终端时退回现在的后台运行，并在诊断中说明。
- `gnome-terminal` 新开的标签页不一定继承 HelloUtau 的环境变量。渲染脚本因此不得依赖继承的环境，脚本中的路径都是绝对路径，工作目录由辅助程序设定。

### macOS 的终端程序

使用 Terminal.app。两种启动方式待实测，见「待作者决定」：

- **AppleScript**：以 `osascript` 执行 `do script`，参数以 `on run argv` 传入，用 `quoted form of` 引用，不拼入脚本源码。可以按窗口 id 只关闭本次打开的窗口。首次使用时系统询问是否允许 HelloUtau 控制 Terminal，用户拒绝后该方式不再可用，需要退回后台运行。
- **LaunchServices**：写出一个带执行权限的 `.command` 文件，内容为辅助程序的命令行，以 `open -a Terminal <文件>` 打开。不需要自动化权限。脚本结束后窗口是否关闭取决于用户的 Terminal 描述文件设置（「当 shell 退出时」）。

两种方式下 Terminal 都以登录 shell 启动，不继承 HelloUtau 的环境，要求与 Linux 相同。

### 安全

- 所有进程都以参数向量启动，不经 shell 拼接命令行。单个字符串形式的终端与 AppleScript 中的引用见上文。
- 传给终端的参数不含工程中的字符串。渲染脚本的内容仍按 `Synth.md`「两种引用模式与安全默认值」生成，本设计不改变脚本。
- 辅助程序只执行 `--` 之后的参数向量，不解释其他输入。socket 上只接收 token 校验，HelloUtau 不向辅助程序发送命令，取消直接向进程组发送信号。

## 测试

- 辅助程序的协议以 Python 测试，由 ctest 调用（CLAUDE.md「构建与验证」中以管道通信程序的例外，这里换为 socket）：正常结束、非零返回码、被信号终止、无法启动、token 不符、`SIGHUP` 时结束子进程组。
- 启动终端的部分以一个假的「终端」程序测试，它只执行收到的参数向量，经设置项指定，因此 CI 中不需要真实的终端。另测单个字符串形式的引用，参数中包含空格、单引号与非 ASCII 字符。
- 真实终端由作者在 macOS 与 Linux 上查看：窗口出现、输出可见、结束后窗口关闭、取消与超时结束整个进程组。

## 后续

- 经典插件宿主在 macOS 与 Linux 上同样以 `QProcess` 在后台运行插件（[`ClassicPluginHost.md`](ClassicPluginHost.md)），Windows 上则有可见的控制台。该程序可以复用于插件，以后再定。

## 待作者决定

1. macOS 采用 AppleScript 还是 LaunchServices，待在 mac 上实测授权提示与窗口关闭的行为后决定。
2. macOS 与 Linux 是否默认在终端中运行，还是作为设置项默认关闭。
3. 脚本以非零值结束时，窗口是否保留到用户按键（Windows 的控制台不保留）。
4. 启动终端的部分在 hellokit 中的名称与所在子库。

## 参考

- **Qt Creator**（<https://github.com/qt-creator/qt-creator>，提交 `017979d`，副本位于 `.cache/qt-creator`，仅检出三个目录）：`src/libs/utils/terminalcommand.cpp`（已知终端的表与查找顺序）、`src/libs/utils/terminalinterface.cpp` 与 `src/tools/process_stub/main.cpp`（辅助程序经本地 socket 报告 `pid` 与 `exit`，socket 位于私有目录）、`src/libs/utils/externalterminalprocessimpl.cpp`（macOS 的 AppleScript，按窗口 id 关闭窗口，以及 macOS 14 上取不到窗口 id 的处理）。Qt Creator 采用 GPL-3.0 加 Qt 例外，**只参考设计，不复制代码**。
- **QSynthesis**（`.cache/QSynthesis-Old/QSynthesis/Frontend/Dialogs/Extern/TerminalDialog.cpp`）：macOS 以 AppleScript 在 Terminal 中执行 `cd <目录> && ./temp.sh && exit`，每秒查询 `busy`。不沿用的原因有五项。取消时退出整个 Terminal，用户的其他会话随之关闭。路径未加引号即拼入 AppleScript 与 shell 命令。`./temp.sh` 要求执行权限。脚本失败时不执行 `exit`，窗口一直保留。拿不到脚本的返回码。Linux 上以 `/bin/bash` 在后台运行，不开窗口。
