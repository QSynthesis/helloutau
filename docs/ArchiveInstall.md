# 安装包

> 状态：设计，作者 2026-10-17 决定。UTAU 对 `type=voiceset` 的处理待实测（「UTAU 的行为」），实测之前不实现依赖它的部分。

本文档定义音源与插件安装包（`.uar`、`.zip`）的安装。安全要求见 [`../CLAUDE.md`](../CLAUDE.md)「安全底线」第二条，本文档只补充安装特有的部分。

## UTAU 的行为

- **`.uar` 是改了扩展名的 zip。** 插件规格原文：「uarというのは、UTAUインストールアーカイブ専用の拡張子で、実体はzipで圧縮したものの拡張子を変更したものです。拡張子はzipのままでもインストール出来ます。」（<https://w.atwiki.jp/utaou/pages/64.html>，原始出处为 <http://utau2008.blog47.fc2.com/blog-entry-379.html>）。
- **`install.txt` 的规格只写了插件。** 原文给出四个键：`type`（插件为 `editplugin`）、`folder`（在 `plugins` 下创建的文件夹名，不可省略）、`contentsdir`（包中要展开的文件夹名，省略时同 `folder`）与 `description`（可省略，一行说明）。包的根上是 `install.txt` 与 `contentsdir` 所指的文件夹。
- **`type=voiceset` 没有文档**，它只以字符串形式出现在 utau.exe 中，与 `editplugin` 相邻。以下由 `.cache/utau-probe/install` 的探针实测（2026-10-17，作者以 UTAU 0.4.18 实测，0.4.19 只修正了路径穿越）：
  - 没有 `install.txt` 时拒绝安装，提示「安装信息未找到 / 这个文件并不是用于UTAU自动安装」。`type` 不是 `voiceset` 或 `editplugin` 时提示「未知的安装文件」。
  - `contentsdir` 指向包里不存在的文件夹时提示「安装信息中有错误」。只写 `folder` 而文件夹同名时，`voiceset` 同样提示「安装信息中有错误」，`editplugin` 提示「插件信息并未找到。」，补上同名的 `contentsdir` 后可以安装。省略 `contentsdir` 而音源文件或 `plugin.txt` 放在根上时可以安装，装出的是根上除 `install.txt` 以外的文件。因此省略 `contentsdir` 时 UTAU 取包的根，并不像规格所写的那样取 `folder`。装出的内容的根上必须有 `character.txt`（`voiceset`）或 `plugin.txt`（`editplugin`），确认框中的名字取自这两个文件，没有时分别报告上述两种错误。
  - `install.txt` 按系统 ANSI 代码页解码：Shift_JIS 的日文说明在 GBK 系统上显示为乱码。
  - 安装前的确认框显示音源的 `character.txt` 中的 `name` 与 `description`，插件则显示 `plugin.txt` 中的 `name`。
  - 目标文件夹已存在时询问是否覆盖，同时显示文件夹名与已安装音源的名字。选「否」时改装到在文件夹名后加数字的新文件夹（`probe1-folder0`）。覆盖为合并：同名文件被替换，原有的其他文件保留。
  - 音源装到 `%AppData%\UTAU\voice\<folder>`，插件装到 `%AppData%\UTAU\plugins\<folder>`，不是 UTAU 安装目录下的 `voice` 与 `plugins`。`install.txt` 不复制。
  - 不带 UTF-8 标志的条目名按系统 ANSI 代码页解码：Shift_JIS 的 `あ.wav` 在 GBK 系统上装成 `偁.wav`，即 [`Editing.md`](Editing.md)「文件名编码错解的处理」所述的错解。带 UTF-8 标志的条目名按 UTF-8 解码，`你.wav` 这类 Shift_JIS 无法表示的名字也正确。
  - 每个装好的音源文件夹中多出一个 `$read` 文件，内容为 28 个随机大写字母加 CRLF。UTAU 安装目录与手动放入的音源中也有这个文件，用途未查证，HelloUtau 不写它。
- 实际的 `install.txt` 不一定是 Shift_JIS（2026-10-17 实测）：frqeditor 20160417 的 `install.txt` 是 Shift_JIS，作者本机 PitTrace 副本的 `install.txt` 只能按 GBK 解码，内容为「◆拡張ピッチエディタ Version 2.15」。后者是本机的副本，不能说明原始发布包的编码，但说明 `install.txt` 的编码同样须由用户选择。

OpenUtau 的做法可作对照（`OpenUtau.Core/Classic/VoicebankInstaller.cs`）：条目名的编码由用户在向导中选择，默认 Shift_JIS，并预览解码结果。它不读 `install.txt`，只跳过根上的 `install.txt`。文件在根上时，它装进以压缩包名命名的文件夹。它唯一的路径检查是跳过名字含 `..` 的条目，其余照常安装。HelloUtau 不沿用这一检查，按安全底线拒绝整个压缩包。

## 范围

- 音源与插件都安装（作者 2026-10-17 决定）。`install.txt` 的 `type` 为 `editplugin` 时装到用户的 UTAU 插件目录（`Documents/OpenVPI/HelloUtau/ClassicExtensions/plugins`，见 [`Distribution.md`](Distribution.md)），`voiceset` 按音源装到 `Documents/OpenVPI/HelloUtau/ClassicExtensions/voice`，与 UTAU 装入 `%AppData%\UTAU` 下的同名目录相对应。没有 `install.txt` 的包与未知的 `type` 同 UTAU 拒绝安装（作者 2026-10-17 决定）。
- 入口：把 `.uar` 或 `.zip` 拖到工程窗口或音源窗口，或者「文件 → Install Voice Bank or Plugin...」。拖入的 `.zip` 只在根上有 `install.txt` 时视为安装包，否则按原有的拖放处理。`.uar` 与从菜单选择的文件根上没有 `install.txt` 时拒绝安装并告知用户。
- 不支持 rar、7z 与嵌套的压缩包。

## 流程

1. **读取条目。** 只读取中央目录，不解压。得到每个条目的原始名字、UTF-8 标志、声明的大小与类型。此时即进行与解压相同的全部检查（见「安全」），不通过时拒绝整个压缩包并告知用户，不进入下一步。
2. **条目名的编码。** 条目名全为 ASCII 或都带 UTF-8 标志时不询问。否则在安装对话框中选择编码：候选为 `TextCodec::ustCandidates()`，默认值按 `TextCodec::ranked()` 选出，旁边以所选编码解码出的目录树预览。某个候选对某个名字是非法字节序列时，该候选不可选。不检测编码，见 [`note.md`](note.md)。
3. **种类与目标文件夹。** 按根上的 `install.txt` 的 `type`、`folder` 与 `contentsdir` 确定：`contentsdir` 所指的文件夹装为 `folder`，省略时包的根装为 `folder`，同 UTAU 不复制 `install.txt`。同 UTAU 要求内容的根上有 `character.txt` 或 `plugin.txt`，否则拒绝安装。
4. **文本编码。** 音源的 `character.txt`、`oto.ini` 等文本文件的编码在同一个对话框中选择，预览沿用 `VoiceBankDecodingDialog` 的文本预览。安装后为每个含 `oto.ini` 的文件夹写入 `hello-config.json`，记录文本编码与文件名编码（见第 5 步），打开该音源时不再询问。插件的 `plugin.txt` 照 UTAU 按系统 ANSI 代码页读取（[`note.md`](note.md)「插件」），不写 `plugin.json`。
5. **名字写入磁盘的方式**（作者 2026-10-17 决定，默认第一种，用户可选第二种）：
   - **按所选编码还原**：磁盘上是正确的 Unicode 名字，`fileNameEncoding` 记为 `{}`。在 Windows 上，有名字无法以系统 ANSI 代码页表示时提示用户：官方 UTAU 在这台机器上无法使用这些文件。
   - **按本机代码页错解**：与 UTAU 在本机解压的结果相同，官方 UTAU 在这台机器上可以使用。`fileNameEncoding` 记为所选编码与系统 ANSI 代码页这一对，HelloUtau 按 [`Editing.md`](Editing.md)「文件名编码错解的处理」读取。有名字在系统 ANSI 代码页中是非法字节序列时，此项不可选，因为错解后的名字无法还原。所选编码与系统 ANSI 代码页相同时两种方式结果相同，不显示此选项。
6. **同名文件夹。** 目标文件夹已存在时同 UTAU 询问覆盖、装到新文件夹或取消。覆盖为合并，同名文件被替换，其他文件保留，因此用户生成的频率表与 `hello-config.json` 不会丢失。新文件夹在原名后加从 0 起第一个未被占用的数字。
7. **解压。** 先解压到目标位置的同一目录下的临时文件夹（`.<名字>.installing-<随机>`），全部成功后改名为目标文件夹，覆盖时把各文件移入目标文件夹。解压失败或取消时删除临时文件夹，目标位置不受影响。进度与取消沿用 `ZipExtractor` 的进度回调。
8. **完成。** 音源装好后可直接打开音源窗口，插件装好后由 ClassicPluginHost 的目录监视自动载入。

## 安全

条目的检查由 `kit::ZipExtractor` 完成（见其头文件），在第 1 步的读取与第 7 步的解压中各进行一次，后者针对选定的编码与目标文件夹。安装另外要求：

- **`install.txt` 的值是不可信输入。** `folder` 与 `contentsdir` 都必须是单个安全的路径分量，规则与条目名的分量相同（非空，不是 `.` 或 `..`，不含分隔符、冒号与控制字符，Windows 上不是保留设备名、不以点或空格结尾）。不符合时拒绝整个压缩包。`install.txt` 以第 2 步选定的编码解码，只有 `description` 显示给用户，以纯文本显示。
- **名字冲突。** 解码后按大小写不敏感比较出现重复的条目名，或者一个文件与另一个条目的文件夹同名时，拒绝整个压缩包，因为在 Windows 与 macOS 的默认文件系统上后者会覆盖前者。完全相同的条目名在打开时即被 libzip 拒绝。只是大小写不同的文件夹视为同一个文件夹，同 Windows。
- **Unicode Path 扩展字段（0x7075）。** 字段的 CRC-32 与原始名字一致时，libzip 以字段中的 UTF-8 名字替换原始名字，因此带该字段的条目视同带 UTF-8 标志，替换后的名字照常检查。CRC-32 不一致时 libzip 忽略该字段，仍按原始名字处理（2026-10-18 以 libzip 1.11.4 测试，`test_ZipExtractor`）。
- **上限。** 沿用 `ZipExtractor` 的条目数与大小上限，并统计实际写入的字节数。
- **测试。** 构造的恶意压缩包至少包括以下几类：
  - `folder` 与 `contentsdir` 为 `..`、带分隔符、带盘符、为保留设备名
  - 大小写冲突与完全重复的条目名
  - 带 0x7075 字段且与原名一致、不一致的条目
  - 带 UTF-8 标志而不是合法 UTF-8 的名字
  - 声明大小伪造的条目

## 实现结构

- **`kit::ZipExtractor` 的补充**（`HelloKitSupport`，已实现）：`listEntries()` 只读取中央目录的条目列表（原始名字、UTF-8 标志、大小、类型），`readEntry()` 读取单个条目的内容并限制其大小，实际读出的字节同样计数（用于 `install.txt` 与文本预览），`setSubdirectory()` 只写出某个文件夹并去掉这一层（`contentsdir`，文件夹名按大小写不敏感比较，其外的条目照常检查），`check()` 进行解压的全部检查而不写入，名字冲突以 `DuplicateName` 拒绝。
- **`kit::InstallArchive`**（`HelloKitVoiceBank`，已实现）：`open()` 读取条目与 `install.txt`，`setNameEncoding()` 以选定的编码解码条目名与 `install.txt`、解析并进行全部检查，`contents()` 与 `readContent()` 供预览，`canWriteMisdecoded()` 与 `unrepresentableNames()` 给出两种写入方式的条件，`freeFolderName()` 给出 UTAU 式的新文件夹名，`install()` 解压到临时文件夹后改名或合并，并为音源写入 `hello-config.json`。不依赖 QtWidgets。插件的安装同样由它完成，因为两者共用 `install.txt` 的格式与全部检查。`folder` 与 `contentsdir` 的检查使用 `ZipExtractor::isSafeComponent()`，与条目名的分量是同一套规则。测试为 `test_InstallArchive`。
- **安装对话框**（`HelloUtauEditor`）：条目名编码、文本编码、写入方式、同名文件夹的处理与进度。入口的拖放与菜单动作也在编辑器中。
