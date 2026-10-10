# 发布、安装、更新与卸载

本文档规定 HelloUtau 的发布物、安装包、用户目录、临时目录、自动更新与卸载。作者 2026-10-11 提出要求，本文为方案，尚未实现。macOS 与 Linux 的打包以后另行补充，本文只规定与平台无关的部分和 Windows 的做法。

## 实现状态

「用户目录」已实现（`AppSettings::userDirectory()`），不迁移旧位置中的内容（作者 2026-10-11 决定）。以 `--settings` 指定设置目录时，用户目录即该设置目录，使该目录自成一体，测试同样以此与真实的文档目录隔开。

其余均未实现，现行代码与本文的出入如下：

- 临时目录现由各处自行分配：工程窗口与试合成使用 `QTemporaryDir`，`ClassicPluginRunner` 使用系统临时目录下的 `HelloUtau-XXXXXX`，`ClassicSynthRunner` 与 `ThreadedSynthRunner` 在未指定脚本目录时使用系统临时目录下的 `hellokit-<毫秒数>`。本文改为由临时目录管理器统一分配。
- 发布物的打包脚本不在仓库中（`.cache/claude/tools/package_windows.ps1`），也没有安装包。

## 发布物

每个版本在 GitHub 与 Gitee 各有一个 release，tag 为 `v` 加 `HELLO_VERSION` 的值，例如 `v0.1.6.0`（作者 2026-10-11 决定，带 `v` 前缀的写法更常见）。附件文件名中的 `<version>` 不带 `v`。

附件的文件名：

| 附件 | 文件名 |
|---|---|
| 免安装版 | `helloutau-<version>-<triplet>.zip` |
| 安装包（仅 Windows） | `helloutau-<version>-<triplet>-setup.exe` |
| 校验文件 | `SHA256SUMS` |

- `<version>` 为 `HELLO_VERSION` 的四段版本号。
- `<triplet>` 为 `<架构>-<系统>-<编译器>`：架构为 `x64` 或 `arm64`，系统为 `windows`、`linux` 或 `osx`，编译器为 `msvc`、`gcc` 或 `clang`，例如 `x64-windows-msvc`。架构与系统的写法沿用 vcpkg 的 triplet。编译器必须写出，因为不同编译器构建的插件与主程序的 ABI 不兼容，用户以某一编译器构建的版本只能更新到同一编译器构建的版本。
- 程序编译时由 CMake 写入自己的 triplet，更新时按它挑选附件，不在运行时推断。
- `SHA256SUMS` 每行为一个附件的 SHA-256 摘要与文件名，格式同 `sha256sum` 的输出。更新前必须以它校验下载的附件。
- 附件由本地脚本上传到 Gitee（`.cache/claude/tools/sync_gitee_assets.py`），可能晚于 release 本身出现。客户端只在当前 triplet 的附件与 `SHA256SUMS` 都已存在时，才认为该源上有新版本。

## 目录

### 安装目录

安装目录的布局与现在的免安装版相同：`bin` 存放程序与各库，`lib/plugins/helloutau` 存放本程序的插件，`lib/plugins/Qt` 存放 Qt 的插件，`share/Qt/translations` 存放 Qt 的翻译（`Plugins.md`「Qt 的插件」）。安装目录中不写入任何运行时产生的文件，因此更新时可以整体替换。

### 用户目录

以下路径中的组织名 `OpenVPI` 与应用名 `HelloUtau` 取自 `HELLOUTAU_ORGANIZATION_NAME` 与 `HELLOUTAU_APPLICATION_NAME`。

| 内容 | 位置 | Windows 上的路径 |
|---|---|---|
| 设置、快捷键、菜单布局 | 应用数据目录（`QStandardPaths::AppDataLocation`） | `%APPDATA%\OpenVPI\HelloUtau` |
| HelloUtau 自己的音源 | 文档目录下的 `OpenVPI/HelloUtau/Singers` | `%USERPROFILE%\Documents\OpenVPI\HelloUtau\Singers` |
| 用户安装的 UTAU 插件 | 文档目录下的 `OpenVPI/HelloUtau/ClassicPlugins` | `%USERPROFILE%\Documents\OpenVPI\HelloUtau\ClassicPlugins` |
| 用户安装的原生插件 | 文档目录下的 `OpenVPI/HelloUtau/Extensions/plugins` | `%USERPROFILE%\Documents\OpenVPI\HelloUtau\Extensions\plugins` |
| 临时文件 | 临时目录下的 `OpenVPI/HelloUtau` | `%TEMP%\OpenVPI\HelloUtau` |

- 音源与插件放在文档目录而不是应用数据目录，同 Synthesizer V。这两类文件由用户自行复制、整理和备份，应用数据目录默认隐藏，不便于用户访问。
- 音源目录名为 `Singers`，不使用 `voice`。
- 文档目录下的第一层目录名采用首字母大写的单词（`Singers`、`ClassicPlugins`），面向用户。原生插件的目录沿用安装目录的小写写法（`plugins`，对应安装目录的 `lib/plugins`），因此以 `Extensions` 一层与第一层隔开（作者 2026-10-11 决定）。以后用户安装的其他扩展内容，例如主题与翻译，同样放在 `Extensions` 下，按安装目录的写法命名。
- 文档目录取 `QStandardPaths::DocumentsLocation`。用户把「文档」重定向到其他位置时，路径随之改变。

### 临时目录管理器

所有临时文件都位于一个总目录之下，即 `QDir::tempPath()` 下的 `OpenVPI/HelloUtau`。卸载器因此只需删除这一个目录。

- 管理器是 `HelloKitSupport` 中的一个类，不依赖 QtWidgets，hellokit 的合成运行器与 helloutau 的各窗口共用。它提供两种用法：返回总目录，或在总目录下分配一个唯一的子目录。子目录以对象表示，对象析构时删除该子目录，用法同 `QTemporaryDir`。
- 每个进程在总目录下有一个会话目录，进程的所有子目录都分配在其中。会话目录中放一个 `QLockFile`，进程运行期间持有。能取得其锁的会话目录属于已退出的进程，管理器据此区分正在运行的进程与已退出的进程。
- 进程正常退出时删除自己的会话目录。程序启动时不删除其他会话目录，因为崩溃的进程留下的文件是以后实现崩溃恢复的依据。已退出进程的会话目录何时删除，由崩溃恢复的设计决定。
- 系统可能自行清理临时目录，例如 Windows 的存储感知。崩溃恢复若需要可靠保留的数据，应另行写入应用数据目录，不能只依赖临时目录。
- 现有的五处临时目录（见「实现状态」）全部改为向管理器申请。合成运行器的 `scriptDirectory` 仍可由调用方指定，未指定时向管理器申请，而不是使用 `std::filesystem::temp_directory_path()`。

## 安装包

Windows 的安装包以 Inno Setup 6 生成，脚本参照 diffscope 的 `dist/installer/windows/setup.iss.in`，由 CMake 以 `configure_file()` 填入版本号、路径与名称。

### 标识与安装模式

- `AppId` 一经发布不得更改，因为 Inno Setup 以它命名卸载信息的注册表键（`Software\Microsoft\Windows\CurrentVersion\Uninstall\<AppId>_is1`），并以它判断新的安装包是否为同一程序的升级。取值为全小写的反向域名 `org.openvpi.helloutau`（作者 2026-10-11 决定）。Windows 注册表的键名不区分大小写，大小写对 Inno Setup 没有影响，但 macOS 的 bundle identifier 与 Linux 的应用 ID 将来使用同一个标识，全小写可避免各平台写法不一。
- `PrivilegesRequiredOverridesAllowed=dialog`：由用户在安装时选择「为所有用户安装」或「只为我安装」，同 diffscope。前者为管理员安装模式，安装到 `{commonpf}`，注册表写入 HKLM。后者为非管理员安装模式，安装到 `{userpf}`（Windows 7 及以后为 `%LOCALAPPDATA%\Programs`），注册表写入 HKCU，不需要提升权限。
- `DefaultDirName={autopf}\OpenVPI\HelloUtau`，`{autopf}` 按安装模式取上述两者之一。注册表一律写在 `HKA` 下，`HKA` 在管理员安装模式下为 HKLM，否则为 HKCU。
- `UsePreviousAppDir`、`UsePreviousTasks`、`UsePreviousGroup` 与 `UsePreviousPrivileges` 保持默认值 `yes`：升级时沿用上一次安装的目录、所选任务、开始菜单文件夹与安装模式，不再询问。
- `ArchitecturesAllowed=x64compatible` 与 `ArchitecturesInstallIn64BitMode=x64compatible`。

### 安装内容

- `[Files]` 以 `recursesubdirs` 复制 `cmake --install` 与打包脚本产出的整个目录。
- 开始菜单快捷方式，以及可选的桌面快捷方式（任务，默认不选）。
- 文件关联（任务）：`.usth` 与 `.ust`，按 Inno Setup 向导生成的写法注册 ProgID，在扩展名的 `OpenWithProgids` 下加入该 ProgID，不改写扩展名的默认值，因为 `.ust` 可能已由 UTAU 关联。设置 `ChangesAssociations=yes`，安装与卸载结束时由 Inno Setup 调用 `SHChangeNotify(SHCNE_ASSOCCHANGED, ...)` 通知资源管理器。ProgID 为 `HelloUtau.usth` 与 `HelloUtau.ust`（作者 2026-10-11 决定）。Microsoft 规定的格式为 `<厂商或应用>.<组件>.<版本>`，不需要并存多个版本时省去版本，同 VS Code 的 `<应用名>.<扩展名>`（`build/win32/code.iss` 中的 `{#RegValueName}.go`）。
- 安装模式下的卸载信息由 Inno Setup 写入：`DisplayName`、`DisplayVersion`（取自 `AppVersion`，即 `HELLO_VERSION`）、`Publisher`、`DisplayIcon`、`InstallLocation`、`UninstallString` 等。

### 签名

现阶段不签名，首次运行时 Windows SmartScreen 会提示。以后取得证书时以 `SignTool` 签名安装包，并设 `SignedUninstaller=yes` 签名卸载程序。

## 卸载

卸载程序在开始卸载前显示一页选项，写法同 diffscope 的 `uninstall.iss.in`：

- 「删除设置与临时文件」，默认勾选（作者 2026-10-11 决定，同 diffscope）：删除应用数据目录下的 `OpenVPI\HelloUtau`（`{userappdata}`，若存在则同时删除 `{localappdata}` 下的同名目录），以及 `TMP` 与 `TEMP` 两个环境变量所指目录下的 `OpenVPI\HelloUtau`。各处的 `OpenVPI` 目录删除后为空时一并删除。
- 文档目录下的 `OpenVPI\HelloUtau` 不删除，因为其中是用户的音源与插件，往往是用户从别处取得、自行整理的文件，删除后无法恢复。这一点与 diffscope 不同。卸载脚本中删除目录的代码处须以注释写明这一理由，以免日后照 diffscope 补上。
- 选项只在交互卸载时显示。静默卸载（`/SILENT`、`/VERYSILENT`）不删除任何用户数据。
- 选项只作用于执行卸载的用户。「为所有用户安装」时，其他用户的应用数据与临时文件不受影响。
- 升级不运行卸载程序，因此升级不会删除用户数据。

## 自动更新

软件体积小，一律全量更新，不做增量更新。

### 更新源

设置的「Updates」页提供以下设置：

- 更新源：「自动」「GitHub」「Gitee」，默认为「自动」。「自动」按系统时区判断：`QTimeZone::systemTimeZone().territory()` 为 `QLocale::China` 时使用 Gitee，否则使用 GitHub。Asia/Shanghai 与 Asia/Urumqi 属于 `QLocale::China`，香港、澳门、台湾的时区各有自己的地区，因此只有中国大陆的时区选择 Gitee。Windows 的「China Standard Time」由 Qt 映射到 Asia/Shanghai。
- 启动时检查更新，默认开启，每天至多检查一次。帮助菜单另有「Check for Updates...」，随时手动检查。
- 跳过的版本：用户在提示中选择「跳过此版本」后，启动时不再提示该版本，手动检查仍然提示。

### 检查与下载

1. 以所选源的公开接口取得最新的 release：GitHub 为 `GET /repos/{owner}/{repo}/releases/latest`，Gitee 为 `GET /api/v5/repos/{owner}/{repo}/releases/latest`。公开仓库无须令牌。
2. 去掉 tag 的 `v` 前缀作为版本号，与 `HELLO_VERSION` 逐段比较，不以 `v` 开头的 tag 不视为版本，较新且附件齐全（见「发布物」）时提示，显示 release 的说明。
3. 用户确认后下载附件与 `SHA256SUMS` 到临时目录管理器分配的目录，显示进度。只接受 HTTPS，拒绝重定向到 HTTP。
4. 以 `SHA256SUMS` 校验附件，不符时删除并报告，不安装。

### 安装版与免安装版的区分

若存在 `Software\Microsoft\Windows\CurrentVersion\Uninstall\<AppId>_is1`（先查 HKCU，再查 HKLM），且其 `InstallLocation` 与程序的安装目录相同，则为安装版，否则为免安装版。安装版下载安装包，免安装版下载 zip。

### 安装版的更新

更新即以静默方式运行新版本的安装包，由 Inno Setup 完成文件替换与注册表的更新：

1. 程序按退出时的流程逐个关闭窗口，询问是否保存未保存的工程与音源。用户取消时不更新。
2. 程序以 `/SILENT /SUPPRESSMSGBOXES /NORESTART /NOCLOSEAPPLICATIONS /UPDATE` 启动安装包，然后退出。`/SILENT` 显示进度而不显示向导。`/UPDATE` 是本程序的自定义参数，安装包在 `[Code]` 中遍历 `ParamStr()` 检查它。
3. 安装包在 `InitializeSetup` 中等待程序的互斥量消失，最多等待一定时间，超时则退出并返回失败。程序启动时创建该互斥量。不使用 Inno Setup 的 `CloseApplications` 强制关闭程序，因为程序已在第 1 步正常退出，强制关闭会丢失未保存的修改。
4. 由于沿用上一次的安装目录、任务与安装模式，安装包覆盖原目录的文件。「为所有用户安装」的安装包需要管理员权限，Windows 照常弹出 UAC 提示。
5. Inno Setup 删除旧的卸载信息键并以新的值重建，`DisplayVersion` 随之更新为新版本。文件关联、快捷方式按所选任务重新写入，路径仍指向原安装目录。
6. 安装结束后，`[Run]` 中一项仅在带有 `/UPDATE` 时执行的条目重新启动程序。普通的「安装后运行」条目带 `skipifsilent`，静默安装时不执行。
7. 安装包的退出码不为 0 时（例如 2 为用户取消，5 为安装中取消或中止），下次启动的程序不会是新版本。程序在下次启动时比较版本，未更新时提示更新失败，并提供下载页链接。

Windows 特有的注册表信息因此全部由安装包维护，程序自身不写注册表。

### 免安装版的更新

正在运行的程序文件无法被覆盖，因此由一个独立的更新程序在主程序退出后替换文件。

1. 程序把 hellokit 的更新程序从安装目录复制到临时目录管理器分配的目录中，从那里启动，因为安装目录将被整体替换，更新程序不能运行在其中。
2. 程序检查安装目录的上级目录是否可写。不可写时（例如解压到了 `Program Files`）不自动更新，改为打开下载页。
3. 程序以参数传入：等待退出的进程号、zip 的路径、安装目录、更新完成后启动的程序与参数。然后按第 1 步的流程关闭窗口并退出。
4. 更新程序等待主程序退出，把 zip 解压到安装目录旁的临时目录（`<安装目录>.new`），确认解压成功后，把安装目录改名为 `<安装目录>.old`，再把 `<安装目录>.new` 改名为安装目录，最后删除 `<安装目录>.old`。任何一步失败都恢复原安装目录。
5. 解压必须遵守 `CLAUDE.md`「安全底线」第二条：拒绝绝对路径、盘符、`..` 路径分量与符号链接条目，以 `weakly_canonical` 确认目标位于目标目录之内，并设大小与条目数上限。这段代码与将来安装音源、插件压缩包的代码共用。
6. 更新程序启动新版本后退出。

安装目录在更新前后路径不变，用户自行创建的快捷方式与文件关联因此仍然有效。免安装版不写注册表，也没有需要更新的注册表信息。

## 模块划分

- `HelloKitUpdate`（新子库，`hellokit/Update/`，命名空间 `hello::kit`）：与具体程序无关的更新工具。包括取得 release 信息（GitHub 与 Gitee 两种源）、比较四段版本号、按文件名模式与 triplet 挑选附件、下载并以 `SHA256SUMS` 校验。程序名、仓库地址、triplet 与源的选择规则都由调用方传入。依赖 Qt Network，不依赖 QtWidgets。
- 更新程序（`hellokit/tools/updater`）：免安装版的文件替换，参数全部来自命令行，不含任何 HelloUtau 特有的内容。按 `CLAUDE.md`，`main.cpp` 只包含入口，逻辑位于库中以便测试。
- 临时目录管理器：`HelloKitSupport`。
- 解压 zip 使用 libzip（BSD 许可证，vcpkg 有端口，作者 2026-10-11 决定），供更新程序与将来安装音源、插件压缩包的代码共用：
  - 以 `ZIP_FL_ENC_RAW` 取得条目名的原始字节。音源的压缩包常以 Shift_JIS 或 GBK 写条目名而不标 UTF-8，按 `CLAUDE.md`「编码」，这些字节必须交给 `winacp` 解码，不能由解压库自行转换。
  - libzip 文档规定不同的 `zip_t` 对象互相独立，可在多个线程中并行使用而无须加锁。多线程解压时每个工作线程各自打开一次压缩包，分别解压不同的条目。
  - 不采用 bit7z：它在运行时需要随程序提供 7-Zip 的 `7z.dll`，条目名由 7-Zip 解码后以宽字符串给出，无法按本仓库的规则自行解码。
- helloutau：「Updates」设置页、启动时与手动的检查、提示对话框、关闭窗口与退出的流程、安装版与免安装版的判断，以及时区到更新源的规则。

## 参考

- **diffscope**：安装与卸载脚本（`dist/installer/windows/setup.iss.in`、`uninstall.iss.in`），本文的安装模式、文件关联与卸载选项均沿用其写法。
- **Visual Studio Code**（同样使用 Inno Setup）：更新时以 `/verysilent`、`/nocloseapplications` 与自定义的 `/update=` 参数运行新的安装包（`src/vs/platform/update/electron-main/updateService.win32.ts`），以 `checksum` 核对下载文件的 SHA-256。它在程序运行期间把新文件安装到 `{app}\_`，程序退出后由 `inno_updater.exe` 替换（`build/win32/code.iss`、`microsoft/inno-updater`），以实现后台更新。HelloUtau 体积小，不需要后台更新，因此只采用前一半：先退出程序，再静默运行安装包。
- **Notepad++ 的 WinGUp**：通用的更新程序，以 XML 告知新版本与下载地址，下载后运行安装包，并以 `WinVerifyTrust` 校验安装包的 Authenticode 签名（`notepad-plus-plus/wingup`）。HelloUtau 的 `HelloKitUpdate` 与更新程序同样与具体程序无关，以后签名时再加入签名校验。
- **Squirrel.Windows**：安装到 `%LocalAppData%\<应用>`，各版本并列于 `app-<版本>` 目录，由 `Update.exe` 启动，按 `RELEASES` 文件在增量包与全量包之间选择。HelloUtau 只做全量更新，不采用这种布局。
