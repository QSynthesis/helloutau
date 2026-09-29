# 渲染比较脚本（交付 moreloader）

写于 2026-09-29，对应 moreloader 的任务书 `E:\GitHub\moreloader\.cache\claude\helloutau-render-comparison-task.md`。工具是 `ustrender`（`hellokit/tests/manual/ustrender`）新增的三个子命令，构建后位于 `build/<配置>/out/bin/ustrender.exe`。

## 一、用法

### 1. 复制音源

```
ustrender copy-voice <音源目录> <新目录>
```

按原目录结构复制，**不复制**引擎由样本派生的文件。按文件名结尾判断，不区分大小写：

- 频率表：`.frq`、`.frt`、`.frc`、`.pmk`、`.gfrq`、`.dio`、`.vs4ufrq`、`.spec`、`.star`、`.platinum`、`.uspec`；
- moresampler 的模型与数据库：`.llsm`、`.llsm.tmp`、`.mrq`（含 `desc.mrq`）。

其余文件一律保留，包括 wav、`oto.ini`、`prefix.map`、`character.txt`、`readme.txt`。

- 新目录必须不存在或为空，工具不覆盖、不删除任何文件。重复执行时先删除新目录。
- 结束时列出复制的文件数和各类被略去的文件数。
- 对第 3 节的音源，实测复制 469 个文件，略去：
  - `.frq` 435、`.llsm` 434、`.pmk` 199、`.gfrq` 109、`.vs4ufrq` 49；
  - `.frc` 7、`.mrq` 3、`.frt` 2、`.uspec` 2。

两侧的音源应是**同一份副本的拷贝**。Windows 一侧直接使用这份副本，Linux 一侧把它原样复制到 WSL，内容相同。

### 2. 生成脚本

```
ustrender script <工程> <输出文件> --target windows|linux
    --voice <本机可读的音源副本> [--voice-as <脚本中的音源目录>]
    [-c <UST 编码>] [--voice-charset <音源编码>]
    --cache <缓存目录> --script-dir <脚本目录> [--emit-dir <写出位置>]
    [--snapshots <快照目录>]
    --resampler-command <程序> [<参数>...] --wavtool-command <程序> [<参数>...]
    [--last-note]
```

| 参数 | 含义 |
|---|---|
| `<工程>` | `.ust` 或 `.usth`。只读 |
| `<输出文件>` | 工程的 `OutFile` 的替代，按脚本中的写法 |
| `--target` | `windows` 写 `temp.bat`；`linux` 写 `temp.sh` 与一份不执行的 `temp.bat`（见第三节） |
| `--voice` | 本机读取的音源副本。两个目标都从**同一份**副本生成，见第四节 |
| `--voice-as` | 脚本中代替 `--voice` 的目录，Linux 一侧填 WSL 中的路径 |
| `-c` | UST 未声明编码时必填。本工程为 `GBK` |
| `--cache` | 工程 `CacheDir` 的替代 |
| `--script-dir` | 脚本所在目录，也是引擎的工作目录，按脚本中的写法 |
| `--emit-dir` | 本机写出文件的位置，省略时为 `--script-dir`。Linux 一侧填本机目录，再由 moreloader 一侧复制过去 |
| `--snapshots` | 快照目录，省略时不复制 |
| `--resampler-command`、`--wavtool-command` | 参数向量的前缀，到下一个选项为止：Windows 为 exe 的路径，Linux 为 `moreloader` 与 exe 两项 |
| `--last-note` | 在最后一个有声音符的 wavtool 调用末尾加 `LAST_NOTE`，默认不加（见第三节） |

所有路径都按目标系统的分隔符写出：Windows 为 `\`，Linux 为 `/`。在 Git Bash 中调用时，须设 `MSYS_NO_PATHCONV=1`，否则 `/home/...` 会被改写成 Windows 路径。

示例：本机实测所用的命令，`W` 为本机工作目录，`L` 为 WSL 中的目录。

```
ustrender script W/project.ust W/out/out.wav -c GBK --target windows \
    --voice W/voice --cache W/cache --script-dir W/win --snapshots W/snap \
    --resampler-command W/moresampler/moresampler.exe \
    --wavtool-command W/moresampler/moresampler.exe

MSYS_NO_PATHCONV=1 ustrender script W/project.ust L/out/out.wav -c GBK --target linux \
    --voice W/voice --voice-as L/voice --cache L/cache --script-dir L/run \
    --emit-dir W/linux --snapshots L/snap \
    --resampler-command /mnt/e/GitHub/moreloader/build/out/bin/moreloader L/moresampler/moresampler.exe \
    --wavtool-command /mnt/e/GitHub/moreloader/build/out/bin/moreloader L/moresampler/moresampler.exe
```

### 3. 自检

```
ustrender compare-manifests <清单一> <清单二>
```

- **路径根的替换**：把各清单的 `output`、`log`、`voice`、`cache`、`scriptDirectory`、`snapshotDirectory` 替换为占位符，较长者先替换，分隔符统一为 `/`。
- **比较范围**：`lastNote`、`steps` 与 `finalSnapshots`。
- **结果**：相同时输出 `same: <步骤数> steps`，返回 0；不同时报告第一处不同的步骤、字段和两侧的值，返回 2。
- **本工程的结果**：`same: 203 steps`，即 96 次 resampler、106 次 wavtool、1 次拼接。

## 二、生成的文件

写到 `--emit-dir`（或 `--script-dir`）：

| 文件 | 目标 | 编码与行尾 |
|---|---|---|
| `temp.bat` | windows：执行 | ANSI 代码页（与 `ClassicSynthRunner` 写 `temp.bat` 的规则相同），CRLF。任一字符在代码页中无法表示时报错，不写出 |
| `temp.sh` | linux：执行 | UTF-8，LF |
| `temp.bat` | linux：**不执行**，供 moresampler 读取 | UTF-8，CRLF。内容与 Windows 的 `temp.bat` 相同，只是路径为 Linux 的 |
| `manifest.json` | 两者 | UTF-8 |

运行时，脚本在脚本目录写 `exitcodes.log`，每步一行：`<四位序号> <kind> <退出码>`。缓存已存在而跳过 resampler 时，退出码的位置写 `skipped`；拼接的两个文件缺一时也写 `skipped`。任一步失败都继续执行后续步骤。

快照的文件名为 `<四位序号>_<产物的文件名>`，每步只复制已存在的产物：

- resampler：缓存 wav 与 `<缓存 wav>.llsm.tmp`；
- wavtool：输出文件、`.whd`、`.dat`；
- 拼接：输出文件；
- 全部步骤之后：
  - `final_<输出文件名>`；
  - 每个样本所在目录的 `desc.mrq`，名为 `final_desc<相对目录，/ 换成 _>.mrq`，音源根目录的即 `final_desc.mrq`。

## 三、清单

全局字段：

| 字段 | 含义 |
|---|---|
| `target` | `windows` 或 `linux` |
| `project` | 工程路径，按命令行 |
| `voice`、`cache`、`output`、`scriptDirectory`、`snapshotDirectory`、`log` | 按脚本中的写法。无快照时 `snapshotDirectory` 为 `null` |
| `resamplerCommand`、`wavtoolCommand` | 引擎命令前缀 |
| `lastNote` | 是否使用 `LAST_NOTE` |
| `steps` | 见下 |
| `finalSnapshots` | 全部步骤之后的复制：`[{source, snapshot}]` |

`steps` 的每一项：

| 字段 | 含义 |
|---|---|
| `index` | 步骤序号，从 1 起，与快照文件名、日志一致 |
| `kind` | `resampler`、`wavtool` 或 `concatenate` |
| `note` | 音符在轨道中的下标，从 0 起。拼接为 `null` |
| `arguments` | 传给引擎的参数向量，不含命令前缀，原样。拼接为空 |
| `outputs` | 本步骤的产物路径 |
| `snapshots` | 与 `outputs` 一一对应的快照路径；无快照时为空 |

## 四、约定与注意事项

- **工作目录与 `temp.bat`**：moresampler 作为 wavtool 时，按 `<当前工作目录>\temp.bat` 中最后一个 `@set temp=` 行判断是否为最后一个音符，据此写出最终的 wav（见 [`moresampler-temp-bat.md`](moresampler-temp-bat.md)）。两个脚本开头都先切换到脚本目录。moreloader 须保证：
  - 客体经 `_wgetcwd()` 取得的工作目录拼上 `\temp.bat` 后，映射到脚本目录中的那份 `temp.bat`。
  - 该行中的路径用哪种写法都不影响判断，moresampler 只取文件名开头的音符序号。
- **`LAST_NOTE`**：`--last-note` 是另一种触发方式，不依赖 `temp.bat`。默认不用，以便走与 UTAU 相同的路径。
- **两侧的处理器数须相同**（moreloader 一侧实测）：moresampler 分析样本时的 OpenMP 线程数等于进程可用的处理器数，与 `multithread-synthesis` 无关。主线程与工作线程的 x87 精度不同，第一段由主线程计算、段长随线程数而变，所以输出随处理器数而变。同一台 Windows 机器限定 8 个处理器与用 16 个时，结果从第 3 步起不同。比较前先让两侧可用的处理器数相同：
  - Windows：`start "" /affinity <十六进制掩码> /wait cmd /c temp.bat`，例如 8 个处理器为 `FF`；
  - Linux：`taskset -c 0-<n-1> sh temp.sh`。
- **缓存文件名**：与 UTAU 相同，由序号、别名、音高和一个摘要组成。摘要含全部 resampler 参数（包括样本路径）以及样本文件的大小与修改时间。
  - 两份脚本都从同一份副本（`--voice`）生成，再以 `--voice-as` 替换路径，因此两侧的缓存名相同。
  - 若分别从两份副本生成，修改时间不同，缓存名也会不同。
- **编码**：本工程的别名为拼音，实测生成的 `temp.bat` 与 `temp.sh` 全为 ASCII，编码不影响结果。含非 ASCII 字符的工程：
  - Windows 脚本按 ANSI 代码页写出；
  - Linux 的两个文件为 UTF-8。
- **休止符**：wavtool 的输入为音源目录下的 `R.wav`（不存在），与 UTAU 的 `%oto%\R.wav` 相同。
- **结尾的休止符**：最后一个有声音符之后的休止符，其 wavtool 调用发生在 moresampler 进入 mode 5、写出最终 wav 之后。实测见第六节：它把一条索引追加在 wav 的末尾，wav 头不变。

## 五、与 UTAU 的 `temp.bat` 的差异

- **调用方式**：调用逐条写出，不经过 `temp_helper.bat`，以便每次调用之后记录退出码并复制快照。调用顺序、参数、缓存名、「缓存已存在则跳过 resampler」、`.whd` 与 `.dat` 的拼接及其后的删除，均与 UTAU 相同。
- **变量**：只写 moresampler 读取的 `@set temp=` 一行。其写法与 UTAU 相同，cmd 的运算符以 `^` 转义，`%` 加倍。UTAU 的其他 `@set`（`tool`、`resamp`、`flag`、`env` 等）不写，参数直接写在调用行上，按目标 shell 逐个转义：bat 按需加双引号并把 `%` 加倍，sh 用单引号。
- **开头**：多出切换目录、清空日志，以及建立输出文件所在目录与快照目录。
  - UTAU 的输出目录总是已存在。moresampler 不会建立它，输出目录不存在时第一次 wavtool 调用会无限地 `wait_lock`（实测）。
  - 原因：追加索引时以 `rb+` 打开输出文件失败，随后对空句柄加锁。

## 六、Windows 实测

2026-09-29，本机 Windows 11。按第一节第 2 步的示例生成脚本，以原生 moresampler 0.8.4 运行 `temp.bat`。moresampler 的副本附带的 `moreconfig.txt` 为 `resampler-compatibility off`、`multithread-synthesis on`；音源副本不含派生文件，缓存目录为空。

- **用时与退出码**：10.6 秒。`exitcodes.log` 中 96 次 resampler 与 106 次 wavtool 的退出码均为 0。拼接为 `skipped`：moresampler 不写 `.whd` 与 `.dat`，UTAU 的 `temp.bat` 在这里同样跳过。moreloader 一侧实测，`resampler-compatibility` 打开时同样不写，拼接在四种组合（兼容开关 × 多线程开关）下都被跳过。
- **最终 wav**：`out.wav` 为 44100 Hz、16 位、单声道，38.284 秒，可正常读取。
- **时长差**：工程共 40.075 秒，差值 1.791 秒即结尾休止符（1920 tick）的长度。
  - 原因：最终 wav 在第 201 步（最后一个有声音符，第 104 号）写出，结尾的休止符在其后才调用。
- **结尾休止符的追加**：第 202 步把 340 字节的索引追加在 wav 末尾，快照 `0201_out.wav` 与 `0202_out.wav` 之间只差这 340 字节。wav 头中的长度不变，播放不受影响。两侧应有相同的表现，比较时须知悉。
- **第 199 步以前**：`out.wav` 是 moresampler 的数据索引，以 UTF-16 记录各片段的绝对路径（与 moreloader 的首轮比较所见相同）。
- **快照**：共 301 个，包括：
  - 96 个缓存 wav 与 96 个 `.llsm.tmp`；
  - 106 次 wavtool 之后的 `out.wav`；
  - 拼接之后的 `out.wav`；
  - `final_out.wav` 与 `final_desc.mrq`。
- **自检**：两份清单去掉路径根后相同（`same: 203 steps`）。

### Linux 实测（WSL，moreloader）

同日，以同一份音源的另一份干净副本、同一套 moresampler 与 `moreconfig.txt`，在 WSL 的 `~/moreloader-compare-render/linux/` 中以 `/mnt/e/GitHub/moreloader/build/out/bin/moreloader` 运行 `temp.sh`。快照由 `.cache/claude/tools/work/compare-render/compare_snapshots.py` 按清单逐步比较：wav 逐样本比较，其余逐字节比较，路径根先换成占位符。

wavtool 的索引以 UTF-16LE 内嵌绝对路径，各以 NUL 结尾（Windows 为 `E:\…`，Wine 下为 `Z:\home\…`）。运行目录与清单不符时，例如运行后改了目录名，只替换路径根就不够，每个索引都会被报告为不同（moreloader 一侧见到 107 个）。因此脚本另把每个内嵌绝对路径的目录部分换成 `<dir>`，保留文件名，比较不再依赖目录名的长度。

**`temp.sh` 的行为与 `temp.bat` 相同**：
- 用时 22 秒，96 次 resampler 与 106 次 wavtool 的退出码全为 0，拼接为 `skipped`，快照同样 301 个；
- 第 1 至 200 步 wavtool 写出的索引、全部 96 个缓存 wav 与两侧音源分析出的 45 个 `.llsm` 逐字节相同；
- 结尾休止符同样在 mode 5 之后追加索引，替换路径根后两侧相同；
- `desc.mrq` 的 45 处差异都是每个条目中 2 字节的时间戳。

**唯一的差异在 moresampler 的合成结果**：6 次 resampler 调用的 `.llsm.tmp` 不同，Linux 一侧短 85 至 145 字节，都从第 165 字节起不同。这 6 次调用用到的样本只有三个：`ei_.wav`（第 16、104 步，同一片段）、`ai_.wav`（第 162 步）与 `ong_.wav`（第 181、190、196 步）。
- 用同类样本 `o_.wav` 的调用两侧相同，从参数上看不出这三个样本有何特别。
- 最终 wav 从第 79059 个样本（1.79 秒，第 8 号音符 `_ei`）起不同，是这一差异的后果。
- **是确定性的偏差，而非随机波动**：在 WSL 中把第 16 步的调用单独运行两次，两次结果逐字节相同，也与完整运行时相同，但与 Windows 不同。Windows 上连续两次完整运行的结果也逐字节相同。这是 moreloader 模拟行为上的偏差，由 moreloader 一侧查明。
- **原因与修复（moreloader 一侧，同日）**：这 6 次调用的音高曲线恰好以 `/` 开头（音高字符串的字母表含 `/`），加载器把它当作主机路径改写了。修复后以干净的音源副本重跑 `temp.sh`，`compare_snapshots.py` 报告 203 步全部相同，最终 wav 相同，`desc.mrq` 只有时间戳不同。

### 两点提醒（来自 moreloader 一侧）

- 从 Git Bash 直接运行 Windows 版 moresampler 时须设 `MSYS_NO_PATHCONV=1`，否则以 `/` 开头的参数（包括音高曲线）同样会被 Git Bash 改写。经 cmd 运行 `temp.bat`（如本机实测）不受影响。
- 复制音源须保留修改时间（Linux 用 `cp -p`），否则 moresampler 可能判定 wav 比 `.llsm` 新而重新分析（`auto-update-llsm-mrq on`）。
  - moresampler 以 `_wstat` 比较 wav 与 `.llsm` 的修改时间，还比较 `desc.mrq` 条目的时间戳。wav 较新时报告「The .wav file is newer than the data record」并重新分析，第一次运行的输出因而不同。
  - `copy-voice` 复制每个文件后显式设置同样的修改时间：`std::filesystem::copy_file` 只在 Windows 上（经 `CopyFile`）保留它。本机实测，一份音源的 435 个 wav 与 `oto.ini` 的修改时间全部相同。
  - 把副本复制到 WSL 时须用 `cp -rp`。

### 在 UTAU 中的对照

以同一首工程及其前 9 个音符（带与不带结尾休止符）在 UTAU 中渲染，Tool1 与 Tool2 都是 moresampler 副本，音源为不含派生文件的副本：

| 工程 | 音频 | 与工程总长之差 | wav 末尾多出 |
|---|---|---|---|
| 前 9 个音符，末尾为有声音符 | 2.015 秒 | 0 | 0 字节 |
| 同上，加结尾休止符（1920 tick） | 2.015 秒 | −1.791 秒 | 338 字节 |
| 整首 | 38.284 秒 | −1.791 秒 | 330 字节 |

- **结尾休止符的表现与 helloutau 的脚本相同**：不参与合成，并在 wav 末尾追加一条索引，即「输出路径、`R.wav`、各参数」。这是 UTAU 加 moresampler 本身的行为。字节数随路径长度而变。
- **UTAU 的 `temp.bat` 写法**：`@set temp="%cachedir%\<序号>_…wav"`，值带引号且不展开变量。moresampler 只取文件名开头的序号，与本工具的写法效果相同。
- **UTAU 与 helloutau 渲染出的音频不逐字节相同**：这是 [`../Synth.md`](../Synth.md)「与 UTAU 的偏差」中已记录的曲线偏差，以及 UTAU 删除曲线末尾零值所致。两侧 moresampler 对音源的分析逐字节相同，helloutau 的脚本重复运行结果也逐字节相同。
  - moreloader 的比较两侧都使用本工具的脚本，不受此影响。
- **`utaucompare` 的解析问题（已修正）**：
  - **原因**：`utaucompare` 原先按展开后的程序路径判断一行调用的是哪个引擎。本次 Tool1 与 Tool2 是同一个 moresampler，wavtool 一行因此被当作 resampler 一行。
  - **修正**：改为按原文所调用的变量（`%resamp%`、`%tool%`）判断；路径字段也不再因分隔符不同而报差异。
  - **修正后的结果**：与 Synth.md 的记录一致，8673 个读数，均值 0.437 音分，最大 15 音分，17 个音符逐点相同。参数上只剩两处有意的差异：缓存名，以及第 25 号音符的 `0Q134`。

实测中发现并已修正的两处，见第四、五节：
- 输出目录不存在时 moresampler 无限等锁，现由脚本先建立该目录；
- 休止符原先传给 wavtool 的是不存在的缓存名，现改为与 UTAU 相同的 `R.wav`。这一处不是等锁的原因。
