# 安装包

> 状态：设计，作者 2026-10-17 决定。UTAU 对 `type=voiceset` 的处理待实测（「UTAU 的行为」），实测之前不实现依赖它的部分。

本文档定义音源与插件安装包（`.uar`、`.zip`）的安装。安全要求见 [`../CLAUDE.md`](../CLAUDE.md)「安全底线」第二条，本文档只补充安装特有的部分。

## UTAU 的行为

- **`.uar` 是改了扩展名的 zip。** 插件规格原文：「uarというのは、UTAUインストールアーカイブ専用の拡張子で、実体はzipで圧縮したものの拡張子を変更したものです。拡張子はzipのままでもインストール出来ます。」（<https://w.atwiki.jp/utaou/pages/64.html>，原始出处为 <http://utau2008.blog47.fc2.com/blog-entry-379.html>）。
- **`install.txt` 的规格只写了插件。** 原文给出四个键：`type`（插件为 `editplugin`）、`folder`（在 `plugins` 下创建的文件夹名，不可省略）、`contentsdir`（包中要展开的文件夹名，省略时同 `folder`）与 `description`（可省略，一行说明）。包的根上是 `install.txt` 与 `contentsdir` 所指的文件夹。
- **`type=voiceset` 没有文档**，它只以字符串形式出现在 utau.exe 中，与 `editplugin` 相邻。以下由 `.cache/utau-probe/install` 的探针实测（2026-10-17，作者以 UTAU 0.4.18 实测，0.4.19 只修正了路径穿越）：
  - 没有 `install.txt` 时拒绝安装，提示「安装信息未找到 / 这个文件并不是用于UTAU自动安装」。`type` 不是 `voiceset` 或 `editplugin` 时提示「未知的安装文件」。
  - `contentsdir` 指向包里不存在的文件夹时提示「安装信息中有错误」。只写 `folder` 而文件夹同名时，`voiceset` 同样提示「安装信息中有错误」，`editplugin` 提示「插件信息并未找到。」，补上同名的 `contentsdir` 后可以安装。因此省略 `contentsdir` 时 UTAU 并不像规格所写的那样取 `folder`，是否改取包的根待实测。
  - `install.txt` 按系统 ANSI 代码页解码：Shift_JIS 的日文说明在 GBK 系统上显示为乱码。
  - 安装前的确认框显示音源的 `character.txt` 中的 `name` 与 `description`，插件则显示 `plugin.txt` 中的 `name`。
  - 目标文件夹已存在时询问是否覆盖，同时显示文件夹名与已安装音源的名字。选「否」时改装到在文件夹名后加数字的新文件夹（`probe1-folder0`）。覆盖是替换还是合并待第二轮确认。
  - 安装位置与条目名的解码待第二轮确认。
- 实际的 `install.txt` 不一定是 Shift_JIS（2026-10-17 实测）：frqeditor 20160417 的 `install.txt` 是 Shift_JIS，作者本机 PitTrace 副本的 `install.txt` 只能按 GBK 解码，内容为「◆拡張ピッチエディタ Version 2.15」。后者是本机的副本，不能说明原始发布包的编码，但说明 `install.txt` 的编码同样须由用户选择。

OpenUtau 的做法可作对照（`OpenUtau.Core/Classic/VoicebankInstaller.cs`）：条目名的编码由用户在向导中选择，默认 Shift_JIS，并预览解码结果。它不读 `install.txt`，只跳过根上的 `install.txt`。文件在根上时，它装进以压缩包名命名的文件夹。它唯一的路径检查是跳过名字含 `..` 的条目，其余照常安装。HelloUtau 不沿用这一检查，按安全底线拒绝整个压缩包。

## 范围

- 音源与插件都安装（作者 2026-10-17 决定）。`install.txt` 的 `type` 为 `editplugin` 时装到用户的 UTAU 插件目录（`Documents/OpenVPI/HelloUtau/ClassicPlugins`，见 [`Distribution.md`](Distribution.md)），`voiceset` 按音源装到 `Documents/OpenVPI/HelloUtau/Singers`。没有 `install.txt` 的包与未知的 `type` 同 UTAU 拒绝安装（作者 2026-10-17 决定）。
- 入口：把 `.uar` 或 `.zip` 拖到工程窗口或音源窗口，或者「文件 → Install Voice Bank or Plugin...」。拖入的 `.zip` 只在根上有 `install.txt` 时视为安装包，否则按原有的拖放处理。`.uar` 与从菜单选择的文件根上没有 `install.txt` 时拒绝安装并告知用户。
- 不支持 rar、7z 与嵌套的压缩包。

## 流程

1. **读取条目。** 只读取中央目录，不解压。得到每个条目的原始名字、UTF-8 标志、声明的大小与类型。此时即进行与解压相同的全部检查（见「安全」），不通过时拒绝整个压缩包并告知用户，不进入下一步。
2. **条目名的编码。** 条目名全为 ASCII 或都带 UTF-8 标志时不询问。否则在安装对话框中选择编码：候选为 `TextCodec::ustCandidates()`，默认值按 `TextCodec::ranked()` 选出，旁边以所选编码解码出的目录树预览。某个候选对某个名字是非法字节序列时，该候选不可选。不检测编码，见 [`note.md`](note.md)。
3. **种类与目标文件夹。** 按根上的 `install.txt` 的 `type`、`folder` 与 `contentsdir` 确定：`contentsdir` 所指的文件夹装为 `folder`。省略 `contentsdir` 时的处理与 `install.txt` 是否复制以实测为准。
4. **文本编码。** 音源的 `character.txt`、`oto.ini` 等文本文件的编码在同一个对话框中选择，预览沿用 `VoiceBankDecodingDialog` 的文本预览。安装后为每个含 `oto.ini` 的文件夹写入 `hello-config.json`，记录文本编码与文件名编码（见第 5 步），打开该音源时不再询问。插件的 `plugin.txt` 照 UTAU 按系统 ANSI 代码页读取（[`note.md`](note.md)「插件」），不写 `plugin.json`。
5. **名字写入磁盘的方式**（作者 2026-10-17 决定，默认第一种，用户可选第二种）：
   - **按所选编码还原**：磁盘上是正确的 Unicode 名字，`fileNameEncoding` 记为 `{}`。在 Windows 上，有名字无法以系统 ANSI 代码页表示时提示用户：官方 UTAU 在这台机器上无法使用这些文件。
   - **按本机代码页错解**：与 UTAU 在本机解压的结果相同，官方 UTAU 在这台机器上可以使用。`fileNameEncoding` 记为所选编码与系统 ANSI 代码页这一对，HelloUtau 按 [`Editing.md`](Editing.md)「文件名编码错解的处理」读取。有名字在系统 ANSI 代码页中是非法字节序列时，此项不可选，因为错解后的名字无法还原。所选编码与系统 ANSI 代码页相同时两种方式结果相同，不显示此选项。
6. **同名文件夹。** 目标文件夹已存在时询问覆盖、另起名字或取消，不静默合并。覆盖时先删除原文件夹，再安装。
7. **解压。** 先解压到目标位置的同一目录下的临时文件夹（`.<名字>.installing-<随机>`），全部成功后改名为目标文件夹。失败或取消时删除临时文件夹，目标位置不留下半装的内容。进度与取消沿用 `ZipExtractor` 的进度回调。
8. **完成。** 音源装好后可直接打开音源窗口，插件装好后由 ClassicPluginHost 的目录监视自动载入。

## 安全

条目的检查由 `kit::ZipExtractor` 完成（见其头文件），在第 1 步的读取与第 7 步的解压中各进行一次，后者针对选定的编码与目标文件夹。安装另外要求：

- **`install.txt` 的值是不可信输入。** `folder` 与 `contentsdir` 都必须是单个安全的路径分量，规则与条目名的分量相同（非空，不是 `.` 或 `..`，不含分隔符、冒号与控制字符，Windows 上不是保留设备名、不以点或空格结尾）。不符合时拒绝整个压缩包。`install.txt` 以第 2 步选定的编码解码，只有 `description` 显示给用户，以纯文本显示。
- **名字冲突。** 解码后按大小写不敏感比较出现重复的条目名时，拒绝整个压缩包，因为在 Windows 与 macOS 的默认文件系统上后者会覆盖前者。
- **Unicode Path 扩展字段（0x7075）。** libzip 以该字段中的 UTF-8 名字替换原始名字（字段的 CRC 与原名一致时），因此带该字段的条目视同带 UTF-8 标志。替换后的名字照常检查。字段与原名不一致时 libzip 打开失败，按无法打开的压缩包拒绝。
- **上限。** 沿用 `ZipExtractor` 的条目数与大小上限，并统计实际写入的字节数。
- **测试。** 构造的恶意压缩包至少包括以下几类：
  - `folder` 与 `contentsdir` 为 `..`、带分隔符、带盘符、为保留设备名
  - 大小写冲突与完全重复的条目名
  - 带 0x7075 字段且与原名一致、不一致的条目
  - 带 UTF-8 标志而不是合法 UTF-8 的名字
  - 声明大小伪造的条目

## 实现结构

- **`kit::ZipExtractor` 的补充**（`HelloKitSupport`）：只读取中央目录的条目列表（原始名字、UTF-8 标志、大小、类型），读取单个条目的内容并限制其大小（用于 `install.txt` 与文本预览），只解压某个文件夹并去掉这一层（`contentsdir`），大小写冲突的检查。现有的检查与测试不变。
- **`kit::InstallArchive`**（`HelloKitVoiceBank`）：打开压缩包，解析 `install.txt`，给出种类、目标文件夹名、条目的目录树与需要询问的问题，按选定的编码与写入方式安装。不依赖 QtWidgets，以 `FixedCharsetSelector` 一类的固定回答测试。插件的安装同样由它完成，因为两者共用 `install.txt` 的格式与全部检查。
- **安装对话框**（`HelloUtauEditor`）：条目名编码、文本编码、写入方式、同名文件夹的处理与进度。入口的拖放与菜单动作也在编辑器中。
