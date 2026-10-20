# 发布、安装、更新与卸载

本文档规定 HelloUtau 的发布物、安装包、用户目录、临时目录、自动更新与卸载。作者 2026-10-11 提出要求，本文为方案，已实现的部分见「实现状态」。本文规定与平台无关的部分和 Windows 的做法，macOS 与 Linux 的打包见「macOS 与 Linux 的发布」，两者的自动更新只有计划。

## 实现状态

「用户目录」已实现（`AppSettings::userDirectory()`），不迁移旧位置中的内容（作者 2026-10-11 决定）。以 `--settings` 指定设置目录时，用户目录即该设置目录，使该目录自成一体，测试同样以此与真实的文档目录隔开。

「临时目录管理器」已实现（`kit::TemporaryStorage`）。工程窗口、试合成、`ClassicPluginRunner` 与 `ClassicSynthRunner` 的临时目录都由它分配。`ThreadedSynthRunner` 在未指定脚本目录时不写出脚本，只以总目录作为脚本中的路径，不创建目录。

「自动更新」的检查与下载已实现：`HelloKitUpdate` 的 `SoftwareReleaseQuery` 取得最新的 release，`SoftwareReleaseDownload` 下载附件并以 `SHA256SUMS` 校验。helloutau 的 `UpdateChecker` 在启动时与「Check for Updates...」中检查，更新的全过程由后台的 `UpdateTask` 进行，以模态的 `UpdateDialog` 显示（见「更新对话框」），「Updates」设置页位于 System Settings 之下，提供三档更新方式（`updates/mode`）。

「安装版的更新」已实现：`setup.iss` 的 `/UPDATE` 与 `/RELAUNCH`，程序持有的互斥量（`kit::InnoSetup::holdMutex()`），退出后启动安装包（`kit::ExitLauncher`），以及下次启动时报告结果。

其余均未实现，现行代码与本文的出入如下：

- 「免安装版的更新」已接入主程序，尚未在真实的 release 上运行过完整的流程。

## 发布物

每个版本在 GitHub 与 Gitee 各有一个 release，tag 为 `v` 加 `HELLO_VERSION` 的值，例如 `v0.1.6.0`（作者 2026-10-11 决定，带 `v` 前缀的写法更常见）。附件文件名中的 `<version>` 不带 `v`。

附件的文件名：

| 附件 | 文件名 |
|---|---|
| 免安装版（Windows） | `helloutau-<version>-<triplet>.zip` |
| 免安装版（Linux） | `helloutau-<version>-<triplet>.tar.xz` |
| 安装包（仅 Windows） | `helloutau-<version>-<triplet>-setup.exe` |
| 磁盘映像（仅 macOS） | `helloutau-<version>-<triplet>.dmg` |
| 调试符号 | `helloutau-<version>-<triplet>-symbols.zip`，Linux 为 `-symbols.tar.xz` |
| 校验文件 | `SHA256SUMS` |

- 调试符号供分析崩溃转储使用，按安装目录的结构存放各个 exe 与 dll 的 PDB，包括 vcpkg 依赖的 PDB，Qt 的官方二进制不带 PDB（作者 2026-10-18 要求）。更新程序按完整的文件名挑选附件，不会选中它。MinGW 的构建以 llvm-mingw 生成 PDB，同样收入符号包。macOS 的符号包是各二进制的 dSYM，由 `dsymutil` 从构建的目标文件收集，放在二进制在 bundle 中的相对位置，例如 `HelloUtau.app/Contents/MacOS/HelloUtau.dSYM`。Linux 的符号包是各二进制的 `<文件名>.debug`，由 `objcopy --only-keep-debug` 分离，放在二进制在免安装版中的相对位置，例如 `HelloUtau/bin/helloutau.debug`，二进制只保留符号表与指向它的 `.gnu_debuglink`，解压到免安装版上 gdb 即可找到。没有调试信息的二进制（Qt 的库，以及从二进制缓存恢复、没有目标文件的 vcpkg 依赖）不收入。

- `<version>` 为 `HELLO_VERSION` 的四段版本号。
- `<triplet>` 为 `<架构>-<系统>-<ABI>`：架构为 `x86_64` 或 `arm64`，系统为 `windows`、`linux` 或 `mac`，ABI 为 `msvc`、`mingw`、`gcc` 或 `appleclang`，例如 `x86_64-windows-msvc`。其余编译器取 CMake 编译器 ID 的小写，macOS 上 Xcode 的编译器因此为 `appleclang`，与 vcpkg 沿用的 CMake 编译器 ID 一致，macOS 版为 `arm64-mac-appleclang`（作者 2026-10-16 决定）。vcpkg 的 triplet 本身不含 ABI 段，HelloUtau 仍然保留这一段。架构与系统取用户熟悉的名称，不沿用 vcpkg 的写法（作者 2026-10-17 决定，当时尚未发布过带 triplet 的版本，无须迁移）：架构用 `x86_64` 而不用 `x64`，因为 `x64` 是微软的叫法，`x86_64` 不偏向任何一方；`arm64` 是苹果的叫法，照用；系统用 `mac` 而不用 `osx`，因为不少用户不知道 `osx` 指什么。vcpkg 自己的 triplet（如 `x64-windows`、`arm64-osx`）照旧，只用于安装依赖。ABI 必须写出，因为 ABI 不同的构建之间插件无法载入，用户的版本只能更新到 ABI 相同的版本。写出的是 ABI 而不是编译器（作者 2026-10-11 决定）：clang 以 MSVC 的 ABI 构建时，与 MSVC 的构建互相兼容，同为 `msvc`。CMake 的 `cmake/DetectTriplet.cmake` 求出它，记入缓存项 `HELLO_TRIPLET`，可以 `-DHELLO_TRIPLET=` 覆盖。
- 程序编译时由 CMake 写入自己的 triplet，更新时按它挑选附件，不在运行时推断。
- `SHA256SUMS` 每行为一个附件的 SHA-256 摘要与文件名，格式同 `sha256sum` 的输出。更新前必须以它校验下载的附件。
- 附件由本地脚本上传到 Gitee（`.cache/claude/tools/sync_gitee_assets.py`），可能晚于 release 本身出现。客户端只在当前 triplet 的附件与 `SHA256SUMS` 都已存在时，才把该源视为有新版本。符号包只在 GitHub 上，不上传到 Gitee（作者 2026-10-18 决定），`SHA256SUMS` 中它的一行对更新没有影响，因为更新只读取所选附件的一行。

## 构建渠道

CMake 缓存项 `HELLO_BUILD_CHANNEL` 区分三种构建，正式版与 CI 的构建使用同一套构建与打包的步骤，只有这一项不同（作者 2026-10-16 决定）：

| 渠道 | 来源 | 窗口标题的结尾 | 附件文件名 |
|---|---|---|---|
| `release` | 推送 `v` 开头的 tag 时 CI 的构建 | `HelloUtau 0.1.8.0` | `helloutau-0.1.8.0-x86_64-windows-msvc.zip` |
| `ci` | CI 的其他构建 | `HelloUtau 0.1.8.0-ci.1a2b3c4` | `helloutau-0.1.8.0-ci.1a2b3c4-x86_64-windows-msvc.zip` |
| `dev` | 本地构建，默认值 | `HelloUtau 0.1.8.0-dev.1a2b3c4` | `helloutau-0.1.8.0-dev.1a2b3c4-x86_64-windows-msvc.zip` |

- 渠道写入 `hellokit/Config.h` 的 `HELLOKIT_BUILD_CHANNEL`，提交取 `hellokit/BuildInfo.h` 的 `HELLOKIT_GIT_LAST_COMMIT_HASH` 的前七位。源码不在 git 仓库中时（例如解压的源码包），qmsetup 记录的提交为 `unknown`，名称省略提交，例如 `HelloUtau 0.1.8.0-dev`。程序由 `SoftwareBuildChannel` 读取，工程窗口与音源窗口的标题以它结尾，关于对话框在版本号后注明。`package.ps1` 以它命名附件，并作为安装包的 `AppVerName` 传入，安装向导与「已安装的应用」中的名称因此同样不会被当成正式版。
- 其他渠道的附件名把 `-<渠道>.<提交>` 跟在版本号之后，再接 triplet 与附件种类的后缀，例如 `helloutau-0.1.8.0-ci.1a2b3c4-x86_64-windows-msvc-setup.exe`（作者 2026-10-18 决定）。DiffScope 的开发构建同样把构建标识跟在版本号之后（`<版本>+<标识>`），平台在其后，附件种类在最后。不用 semver 的 `+`，因为它在 URL 中须转义。
- 渠道只影响名称，不影响行为。非正式的构建同样检查更新（作者 2026-10-16 决定）：CI 的构建来自最新的提交，版本号不低于最新的正式版，因此不会被提示更新到旧版本。

## 持续集成

CI 构建、运行全部测试并打包（作者 2026-10-16 提出要求），结构参照 lorelei：

| 文件 | 触发 | 内容 |
|---|---|---|
| `.github/actions/build-and-test/action.yml` | 由下面两个工作流调用 | 按变体安装依赖、配置、编译、运行全部测试，写入运行的摘要 |
| `.github/workflows/ci.yml` | 向 `main` 发起的 pull request、手动 | 三种变体的编译与测试 |
| `.github/workflows/deploy.yml` | 推送 `main`、推送 `v` 开头的 tag、手动 | release 变体的编译、测试与打包，debug 与 asan 变体的编译与测试，tag 时发布 |
| `.github/workflows/sync-gitee-release.yml` | release 发布后由 `deploy.yml` 启动、手动 | 同步到 Gitee |

- **系统**：两个工作流以 matrix 列出各系统，目前为 `x86_64-windows-msvc`、`x86_64-windows-mingw`、`x86_64-linux-gcc` 与 `arm64-mac-appleclang`。组合 action 不限于某一系统，系统的差别由输入（vcpkg 的 triplet、aqt 的 Qt 架构）与 `runner.os` 区分。matrix 中的 triplet 是 job 的名称，组合 action 检查它与构建的 triplet 相同，不同即失败，其后的步骤不使用它：组合 action 在配置后从 `CMakeCache.txt` 读出 `HELLO_TRIPLET`，作为输出 `triplet` 交给工作流命名 artifact，因此与程序、打包脚本所用的值必然一致，将来增加 mingw 等构建时也不会混淆（作者 2026-10-17 要求）。增加一个系统只需在 matrix 中增加一项，并补上该系统的打包步骤。
- **Linux 与 macOS**（作者 2026-10-18 要求）：release 变体在 `deploy.yml` 的 job `package` 中构建、测试并打包（2026-10-19），debug 与 asan 变体在 job `test` 中构建与测试。asan 在 Linux 上由 GCC 构建，LeakSanitizer 照默认开启，在 macOS 上由 Apple clang 构建，都是 CMake 找到的编译器，只有 Windows 指定 clang。macOS 的 asan 不运行 `test_AppLoader`：载入过的测试插件的副本卸载之后，该测试再载入内置插件时，AddressSanitizer 的运行库内部出现 SEGV_ACCERR，关闭 ODR 检查后依然如此。Linux 在 `ubuntu-22.04` 上以 GCC 构建（作者 2026-10-19 决定），免安装版因此要求 glibc 2.35 以上，与 overworld 相同，以后考虑迁到 `ubuntu-24.04`。Qt 为 aqt 的 `linux_gcc_64`，依赖以 overlay 的 triplet `x64-linux` 安装，另以 apt 安装官方 Qt 所需的系统库（OpenGL、EGL、xkbcommon、fontconfig、PulseAudio）。macOS 在 `macos-15` 上以 Apple clang 构建，Qt 为 aqt 的 `clang_64`，triplet 为 `arm64-osx`。两者在 overworld（Ubuntu 22.04，GCC 11.4）与 Apple Silicon 的 Mac（Apple clang 14）上以同样的步骤验证过全部测试（2026-10-18），Linux 的 asan 同样全部通过。Mac 上的 Apple clang 14 在 macOS 26 上启动 AddressSanitizer 即失败（`sanitizer_malloc_mac.inc` 的检查），与本仓库无关，macOS 的 asan 以 CI 的 Xcode 为准。组合 action 的目录不能单独命名为 `build`，因为 `.gitignore` 忽略名为 `build` 的目录。
- **MinGW**：以 llvm-mingw 17.0.6 构建（作者 2026-10-18 决定），即 Qt 6.11.1 的 llvm-mingw 构建（aqt 的架构 `win64_llvm_mingw`）所依赖的工具链 `qt.tools.win64_llvm_mingw1706`，由 install-qt-action 一并安装并加入 `PATH`。程序与 Qt 共用一份 `libc++.dll`，编译器因此与 Qt 的工具链一致。不用 GCC 的 MinGW，因为 GCC 只生成 DWARF，Windows 的调试器与崩溃转储的工具读不了，llvm-mingw 则以 `-gcodeview` 与 `-Wl,--pdb=` 生成 PDB（`cmake/ConfigureBuildTypes.cmake`）。依赖以 vcpkg 社区的 triplet `x64-mingw-dynamic` 安装，vcpkg 按 GCC 的名字在 `PATH` 中找到 llvm-mingw 的编译器。安装时的部署从编译器所在目录与目标的目录（llvm-mingw 以目标三元组命名，如 `x86_64-w64-mingw32/bin`）取得编译器的运行库（`libc++` 与 `libunwind`），不复制 Visual Studio 的运行库。AddressSanitizer 只以 MSVC 的 ABI 构建。依赖 Qt 的 QActionKit 与 substate 仍在开发，没有自己的 CI，它们在 MinGW 下能否构建由本仓库的 CI 检验（作者 2026-10-18 决定）。stdutau 与 winacp 的 CI 以 MSYS2 的 CLANG64 环境检验同一套工具链。
- **分支**：日常提交在 `dev` 上，推送 `dev` 不构建。`git promote`（仓库的 Git 别名，同 qmsetup）推送当前分支，把 `main` 移到同一提交并推送 `main`。推送 `main` 只运行 `deploy.yml`，它在打包前运行同样的测试，因此同一次推送不编译两遍。
- **变体**：组合 action 的输入 `variant` 选择构建的变体，每种变体在自己的构建目录中（作者 2026-10-17 决定）。
  - `release`：Release，即发布的构建，位于 `build/Release`，打包脚本读取这里。Release 带调试信息，同 vcpkg 的 Release 构建（作者 2026-10-18 决定，见 `cmake/ConfigureBuildTypes.cmake`）：MSVC 编译时加 `/Z7`，把调试信息写入目标文件，其他前端加 `-g`。链接选项按 ABI：MSVC 的 ABI（包括以 GNU 前端、MSVC 的 ABI 构建的 clang）链接时加 `/DEBUG /OPT:REF /OPT:ICF`，为每个二进制生成 PDB，并去掉未引用与重复的函数。优化与原来的 Release 相同，符号随附件发布。vcpkg 只构建 Debug 与 Release，`vcpkg_cmake_config_fixup` 只处理 `*-release.cmake`，因此不使用 RelWithDebInfo 这一配置（同 DiffScope 的 overlay triplet 的说明）。vcpkg 在 Windows 上的 Release 本来就以 `/Z7` 编译、以 `/DEBUG` 链接并安装 PDB。
  - `debug`：Debug，带断言与 Debug 运行库的检查，位于 `build/Debug`。
  - `asan`：Release 加 `HELLO_ENABLE_ADDRESS_SANITIZER`，以 AddressSanitizer 检测释放后的访问、堆与栈的越界，位于 `build/asan`。Release 带调试信息，报告中有函数名与行号。Windows 上由 clang（GNU 前端，MSVC 的 ABI）构建，与本地相同（作者 2026-10-17 决定），Linux 与 macOS 见「Linux 与 macOS」。clang 与 MSVC 的运行库同名，测试的步骤把 clang 的运行库目录放在 `PATH` 的最前面。MSVC 14.51 的标准库除 `std::vector` 与 `std::string` 外还为 `std::optional` 加了注解，14.44 没有，因此只关闭前两者时，CI 上 cl 与 clang 的构建都报告 `use-after-poison`，本地都不报告。`cmake/EnableAddressSanitizer.cmake` 改以 `_DISABLE_STL_ANNOTATION` 关闭全部注解。依赖取自 vcpkg，未经插桩，因此关闭了 MSVC 标准库容器的注解，`std::vector` 与 `std::string` 在预留容量内的越界不能发现，见该文件。Release 下崩溃而 Debug 下不崩溃的释放后访问（编辑频率表对话框，`407c21a2`）即是这一变体所针对的问题。
  - `deploy.yml` 的 job `package` 构建 release 并打包，job `test` 同时构建 debug 与 asan。打包不等 `test` 的结果，因此每次推送都产出 CI 的安装包，`test` 失败时整次运行标为失败。tag 的 release 在两个 job 都通过后才创建。`ci.yml` 构建全部三种变体，不打包。
- **依赖**：每次克隆官方 vcpkg 与 `stdware/vcpkg-overlay` 的最新提交到 `build/vcpkg` 与 `build/vcpkg-overlay`，以 classic 模式与 overlay 的 triplet 安装 stdcorelib、stdcorelib.plugin、stdutau、substate、QActionKit、winacp、wolf-midi、r8brain-free-src、libzip、zlib 与 qmsetup。vcpkg 的二进制缓存以 `actions/cache` 保存，输入未变的包不重新构建。Qt 6.11.1 为官方的二进制，由 `jurplel/install-qt-action` 安装。依赖的仓库改动后须先推送，并以 `update-ports` 更新 overlay 的 port，CI 才能用到。
- **测试**：`ctest --no-tests=error`，以 `QT_QPA_PLATFORM=offscreen` 运行界面的测试。release 变体的任何测试失败即不打包。运行的摘要列出渠道、提交、vcpkg 与 overlay 的提交和测试的结果，失败时同样写入。
- **编译缓存**：sccache（`mozilla-actions/sccache-action`），以 GitHub Actions 的缓存逐项保存（作者 2026-10-18 要求）。每次检出的源文件时间都是新的，只靠 Ninja 会全部重新编译，sccache 对预处理结果与选项未变的目标文件直接取用缓存。sccache 不能缓存 MSVC 把调试信息写入共享 PDB（`/Zi`）的目标文件，因此 CI 以 CMP0141 与 `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded` 把调试信息写入目标文件（`/Z7`），链接时仍生成每个二进制的 PDB。`hellokit/BuildInfo.h` 随每次提交改变，只有包含它的少数源文件每次重新编译。运行的摘要附上 sccache 的统计。
- **打包**：Windows 上 Inno Setup 7.1.0 由官方 release 的安装程序静默安装，`package.ps1` 生成 zip、安装包、符号包与 `SHA256SUMS`。macOS 上 `dist/installer/macos/package.sh` 生成 DMG、符号包与 `SHA256SUMS`，Linux 上 `dist/installer/linux/package.sh` 生成 tar.xz、符号包与 `SHA256SUMS`，两者的命名与检查共用 `dist/installer/common.sh`。每个附件是单独的 artifact，以 `actions/upload-artifact` 7 的 `archive: false` 原样上传，下载即是文件本身，与 release 的附件相同，artifact 名即文件名，渠道与提交写在文件名中。各附件的摘要写入运行的摘要。
- **发布**：推送 `v` 开头的 tag 时渠道为 `release`。tag 须与 `HELLO_VERSION` 一致，否则构建失败。各系统都通过后创建 GitHub 的 release，附上各系统的附件（包括符号包，因为 artifact 会过期）与由下载的附件算出的一份 `SHA256SUMS`，标题为 `HelloUtau <version>`，说明取自 tag 所在提交的 `docs/changelog/<version>.md`，该文件不存在时不创建 release。以工作流的令牌创建的 release 不触发其他工作流，因此随后以 `gh workflow run` 启动 `sync-gitee-release.yml`。

## 目录

### 安装目录

安装目录的布局与现在的免安装版相同：`bin` 存放程序与各库，`lib/helloutau/plugins` 存放本程序的插件，`lib/Qt/plugins` 存放 Qt 的插件，`share/helloutau/translations` 存放库的翻译，`share/Qt/translations` 存放 Qt 的翻译（`Plugins.md`「Qt 的插件」）。翻译不嵌入程序，`AppLoader` 在运行时按程序所在位置推算这些目录（作者 2026-10-17 决定）：库的翻译依次在用户目录的 `Extensions/translations` 与程序的 `share/helloutau/translations` 中查找，Qt 的翻译依次在 `share/Qt/translations` 与 Qt 自身的翻译目录中查找。安装目录中不写入任何运行时产生的文件，因此更新时可以整体替换。

### 用户目录

以下路径中的组织名 `OpenVPI` 与应用名 `HelloUtau` 取自 `HELLOUTAU_ORGANIZATION_NAME` 与 `HELLOUTAU_APPLICATION_NAME`。

| 内容 | 位置 | Windows 上的路径 |
|---|---|---|
| 设置、快捷键、菜单布局 | 应用数据目录（`QStandardPaths::AppDataLocation`） | `%APPDATA%\OpenVPI\HelloUtau` |
| HelloUtau 自己的 UTAU 音源 | 文档目录下的 `OpenVPI/HelloUtau/ClassicExtensions/voice` | `%USERPROFILE%\Documents\OpenVPI\HelloUtau\ClassicExtensions\voice` |
| 用户安装的 UTAU 插件 | 文档目录下的 `OpenVPI/HelloUtau/ClassicExtensions/plugins` | `%USERPROFILE%\Documents\OpenVPI\HelloUtau\ClassicExtensions\plugins` |
| 用户安装的原生插件 | 文档目录下的 `OpenVPI/HelloUtau/Extensions/plugins` | `%USERPROFILE%\Documents\OpenVPI\HelloUtau\Extensions\plugins` |
| 用户安装的翻译 | 文档目录下的 `OpenVPI/HelloUtau/Extensions/translations`，先于程序自带的翻译 | `%USERPROFILE%\Documents\OpenVPI\HelloUtau\Extensions\translations` |
| 临时文件 | 临时目录下的 `OpenVPI/HelloUtau` | `%TEMP%\OpenVPI\HelloUtau` |

- 音源与插件放在文档目录而不是应用数据目录，同 Synthesizer V。这两类文件由用户自行复制、整理和备份，应用数据目录默认隐藏，不便于用户访问。
- UTAU 的音源与插件放在 `ClassicExtensions` 下的 `voice` 与 `plugins`，与 UTAU 的安装包装入的 `%AppData%\UTAU\voice`、`%AppData%\UTAU\plugins` 同名（作者 2026-10-17 决定，见 [`ArchiveInstall.md`](ArchiveInstall.md)「UTAU 的行为」），用户可以按 UTAU 的结构整个复制。0.1.7 所用的 `Singers` 与 `ClassicPlugins` 不再使用，不迁移（作者 2026-10-18 决定）。
- 文档目录下的第一层目录名采用首字母大写的单词，面向用户。其下的目录沿用来源的写法：`ClassicExtensions` 下沿用 UTAU 的 `voice` 与 `plugins`，`Extensions` 下沿用安装目录的小写写法（`plugins` 对应安装目录的 `lib/helloutau/plugins`，作者 2026-10-11 决定）。以后用户安装的其他扩展内容，例如主题，同样放在 `Extensions` 下，按安装目录的写法命名。
- 文档目录取 `QStandardPaths::DocumentsLocation`。用户把「文档」重定向到其他位置时，路径随之改变。
- 其他平台的应用数据目录：macOS 为 `~/Library/Application Support/OpenVPI/HelloUtau`，Linux 为 `~/.local/share/OpenVPI/HelloUtau`，与 ds-editor-lite（`fd38fb09`，`SingleInstanceIdentity::defaultDataDirectory()`）相同（作者 2026-10-16 决定）。DiffScope 在 macOS 与 Linux 上用 `~/.config`，未采用。音源与插件在这些平台上同样放在文档目录。

### 临时目录管理器

所有临时文件都位于一个总目录之下，即 `QDir::tempPath()` 下的 `OpenVPI/HelloUtau`。卸载器因此只需删除这一个目录。总目录由 helloutau 在启动时指定，kit 中不出现应用的名称。

- 管理器是 `HelloKitSupport` 中的 `TemporaryStorage`，不依赖 QtWidgets，hellokit 的合成运行器与 helloutau 的各窗口共用。应用像创建 `QCoreApplication` 一样创建它的唯一实例，以总目录为构造参数，各处以 `instance()` 取得。helloutau 的实例由 `AppLoader` 持有，在插件与各窗口之后析构。没有实例时，例如测试与命令行工具，临时目录照旧位于系统临时目录。它不另行实现临时目录，只提供会话目录中的模板路径，调用方以该模板构造 `QTemporaryDir` 或 `QTemporaryFile`，唯一的名称与析构时的删除都由 Qt 完成。
- 每个进程在总目录下有一个会话目录，进程的所有子目录都分配在其中。会话目录中放一个 `QLockFile`，进程运行期间持有。能取得其锁的会话目录属于已退出的进程，管理器据此区分正在运行的进程与已退出的进程。
- 实例析构时删除自己的会话目录，进程正常退出时即如此。程序启动时不删除其他会话目录，因为崩溃的进程留下的文件是以后实现崩溃恢复的依据。已退出进程的会话目录在其中不再有未处理的日志时删除，见 [`Editing.md`](Editing.md)「恢复」（尚未实现）。
- 系统可能自行清理临时目录，例如 Windows 的存储感知。崩溃恢复若需要可靠保留的数据，应另行写入应用数据目录，不能只依赖临时目录。
- 现有的五处临时目录（见「实现状态」）全部改为向管理器申请。合成运行器的 `scriptDirectory` 仍可由调用方指定，未指定时向管理器申请，而不是使用 `std::filesystem::temp_directory_path()`。

## 安装包

Windows 的安装包以 Inno Setup 7 生成，界面语言为英语、简体中文、繁体中文与日语（7.0.2 起两种中文均为官方翻译）。脚本为 `dist/installer/windows/setup.iss`，是可以直接用 Inno Setup 打开和编译的普通脚本，提示文字与卸载页分别在同目录的 `messages.iss` 与 `uninstall.iss` 中。版本号、源目录与输出文件名由命令行的 `/D` 传入，不经 `configure_file()`（作者 2026-10-11 决定）。

同目录的 `package.ps1` 打包一个已完成的 Release 构建：`cmake --install`，删除只供开发者使用的文件，复制 C++ 运行库，收集各个二进制的 PDB，生成 zip、安装包、符号包与 `SHA256SUMS`。它从构建目录的 `CMakeCache.txt` 读取版本号（`CMAKE_PROJECT_VERSION`）与 triplet（`HELLO_TRIPLET`）。脚本不负责构建，因为构建需要编译器的环境。

Qt 与其他依赖库由 `cmake --install` 部署，即 `helloutau/CMakeLists.txt` 末尾的 `qm_deploy_directory()`，与 DiffScope 相同，不用 windeployqt（作者 2026-10-18 决定）。由 `HELLO_DEPLOY` 控制，默认开启，只在 Windows 上部署，Debug 构建不部署，因为 `qm_deploy_directory()` 只部署 release 版的库，`package.ps1` 遇到这两种构建即报错。

- 依赖库按已安装的二进制逐级解析，放在程序旁的 `bin`，在构建的输出目录（`third-party/Dependencies.cmake` 复制依赖库之处）与 Qt 中查找。系统库与 MSVC 的运行库不部署，后者由 `package.ps1` 复制。
- Qt 的插件以 `QT_PLUGINS` 按二进制用到的 Qt 模块选出，放在 `lib/Qt/plugins`，并采用 windeployqt 对不使用 QML 的程序的默认选择：不部署依赖 Qt Quick 的插件（`QT_PLUGINS_EXCLUDE_MODULES Qml Quick`），不部署运行时载入 OpenSSL 的 `tls/qopensslbackend` 与 `generic/qinsighttracker`。
- 结果与 windeployqt 相同：MSVC 下打出的包与原来逐个文件一致（2026-10-18 本地比对）。Direct3D 的着色器编译器 `d3dcompiler_47.dll`、`dxcompiler.dll`、`dxil.dll` 与软件 OpenGL `opengl32sw.dll` 不是链接的依赖，不会被部署，界面以光栅方式绘制，不使用 Qt Quick、OpenGL 控件与 RHI 控件（作者 2026-10-15 同意）。
- 部署在安装时按安装前缀解析目录，`cmake --install --prefix` 因此同样适用，这需要 qmsetup `5b83fa7` 或更新的版本。

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

## macOS 与 Linux 的发布

作者 2026-10-16 决定先完成 Windows 的正式部署。CI 自 2026-10-19 起打包 macOS 与 Linux（作者 2026-10-19 要求），两者的自动更新尚未实现。

- **macOS 的发布物**：只出 arm64，最低系统版本为 13.0，与 Qt 6.11 相同（作者 2026-10-19 决定）。overlay 的 macOS 三元组以 `VCPKG_OSX_DEPLOYMENT_TARGET` 指定同一版本，否则依赖库按编译机 SDK 的版本构建，在较旧的系统上无法载入。`Info.plist` 的 `LSMinimumSystemVersion` 取 `CMAKE_OSX_DEPLOYMENT_TARGET`。`CFBundleLocalizations` 列出英文与 `HELLOUTAU_LANGUAGES` 中的语言：bundle 只声明英文时，macOS 给程序的当前 locale 是英文（系统语言为简体中文时为 `en_CN`，2026-10-20 以探针实测），默认语言因此不是系统语言，翻译不载入，系统的打开与保存面板也是英文。列出之后当前 locale 为 `zh_CN`。以 DMG 发布，不另出 zip。DMG 中为 `HelloUtau.app` 与指向 `/Applications` 的链接，不加背景（作者 2026-10-19 决定），由 `hdiutil create -format UDZO` 生成。打开 DMG 时 Finder 先显示许可协议，用户同意后才挂载（作者 2026-10-19 要求）。协议是 DMG 的软件许可协议资源，由 `dist/installer/macos/sla.py` 从安装的 `LICENSE` 生成，以 `hdiutil udifrez` 写入，hdiutil 报告该命令已弃用，但没有其他写入方式。协议提供英文与简体中文两种语言，Finder 按系统语言选择，没有对应语言时显示英文。两种语言的按钮与说明各自翻译，正文都是英文的 `LICENSE`（作者 2026-10-20 要求以系统语言显示）。各平台的安装内容都带有 `LICENSE`，位于数据目录（`share/helloutau` 或 `Contents/Resources`）。Windows 安装包显示的协议与 DMG 的协议都取自安装的这一份，不读仓库中的文件（作者 2026-10-19 要求）。
- **macOS 的签名**：ad-hoc 签名（`codesign --sign -`），不申请 Apple 的 Developer ID，不公证，同 DiffScope。Apple Silicon 只运行带签名的代码，ad-hoc 签名满足这一条件，但 Gatekeeper 不信任它。用户第一次打开下载的应用时须在「系统设置 > 隐私与安全性」中选择「仍要打开」。部署改写了二进制的运行路径，原有的签名随之失效，打包脚本因此由内向外重新签名：framework 之外的二进制、各个 framework、bundle 本身，再以 `codesign --verify --deep --strict` 检查。
- **bundle 与部署**：参照 DiffScope（`diffscope/diffscope-project`，`eb92abd4`）。bundle 由 qmsetup 的 `qm_add_mac_bundle()` 与 Info.plist 模板生成，模板声明 `.usth` 与 `.ust` 的文档类型。icns 为提交的 `helloutau.icns`。Qt 与依赖以 `qm_deploy_directory()` 部署到 `Contents/Frameworks`，Qt 的插件放在 `Contents/Frameworks/Qt/plugins`，不用 macdeployqt。HelloUtau 另有以下几处：
  - Qt 的插件目录由 `AppLoader::addQtPluginPaths()` 按 bundle 求出，不用 `qt.conf`（作者 2026-10-01 决定）。bundle 中为 `Contents/Frameworks/Qt/plugins`，而不是 Qt 自己会查找的 `Contents/PlugIns`，因为 qmsetup 的布局以 `Contents/Plugins` 存放应用的插件，两者在不区分大小写的文件系统上是同一个目录（作者 2026-10-17 决定）。
  - 翻译按 bundle 求出（作者 2026-10-17 决定）：库的翻译在 `Contents/Resources/translations`（qmsetup 的 data 目录），Qt 的翻译在 `Contents/Resources/Qt/translations`，分别对应其他系统的 `share/helloutau/translations` 与 `share/Qt/translations`。更新配置同样在 data 目录，由 `AppLoader::applicationDataPath()` 求出。hellokit 的辅助程序与主程序同在 `Contents/MacOS`。
  - 两个模块的 `<模块>_MACOSX_BUNDLE_NAME` 取根目录的 `HELLO_MACOSX_BUNDLE_NAME`，即 `HelloUtau`，该名字在其他系统上不起作用。不构建应用时同样按 bundle 布局（作者 2026-10-19 决定）。
  - `cmake --install` 整体复制构建中的 bundle，因此不随应用发布的文件不得输出到 bundle 中：测试与只供测试的库以 qmsetup 的 `TEST` 输出到 `out/bin` 与 `out/lib`。静态库的归档与其他库同在 `Contents/Frameworks`，不安装开发文件（`HELLO_DEVEL` 关闭）时，qmsetup 在复制 bundle 之后、部署重新签名之前把它们从安装的 bundle 中删除，因为 codesign 拒绝含有归档的 bundle。测试启动的辅助程序由 `PROGRAMS` 以路径传给测试，不复制到测试旁边（见 `cmake/AddAutoTest.cmake`）。
  - 从 Finder 打开文件经 `QFileOpenEvent` 传入，由 `AppLoader` 处理。插件初始化之前收到的文件并入命令行的文件，由核心插件打开。之后收到的文件由编辑器打开：唯一的窗口是启动时新建、未经编辑的工程时，文件在该窗口中打开，否则新开窗口。
- **macOS 的自动更新**：安装即把 `.app` 拖入「应用程序」文件夹，没有安装记录，因此更新以 `.app` 为单位整体替换。程序下载 DMG，以 `hdiutil attach` 挂载，退出后由 `hello-updater` 以新的 `.app` 替换旧的，再启动新版本，与 Sparkle 的做法相同。程序自己下载的文件不带隔离属性，更新后应不再出现 Gatekeeper 的提示，这一点尚未实测。须处理两种情形：
  - 对 `.app` 所在的文件夹没有写权限，例如标准用户的「应用程序」文件夹，须请求授权，或提示用户手动更新。
  - 直接从 DMG 或「下载」文件夹运行的应用会被系统移到只读的随机路径运行（App Translocation），无法更新，须检测出来并提示用户先把应用移到「应用程序」文件夹。
- **Linux**：只出 tar.xz 压缩包，即 `helloutau-<version>-x86_64-linux-gcc.tar.xz`，不出 AppImage 与发行版的包（作者 2026-10-16 决定）。最初为 zip，作者 2026-10-19 改为 tar.xz，因为 tar.xz 是 Linux 上通行的格式，原生保存文件权限与库的符号链接，压缩率也高于 zip。符号包同为 tar.xz。条目的属主写为 0，不带构建机的用户名。以 `ubuntu-22.04` 构建，要求 glibc 2.35 以上。免安装版的布局同 Windows：`bin` 为程序，`lib` 为库，`lib/Qt/plugins` 为 Qt 的插件，`share` 为数据。系统库目录（`/lib`、`/usr/lib` 及其子目录）中的库由系统提供，不随包发布，例如显卡驱动的 libGL 与 libxcb。Qt 自带的 ICU 随包发布（qmsetup 的 `--standard`）。Wayland 与 EGL 的平台配套插件目前随包发布，以后由 qmsetup 按所选的平台插件排除。自动更新只检查不安装，发现新版本时打开下载页（作者 2026-10-18 决定），尚未实现。

## 应用图标

图标是一条蓝色的粗缎带：卷帘上一低一高两个音符由滑音连成一笔，滑音两端的控制点是缎带上挖空的圆孔，放在浅色的圆角方形底板上（作者 2026-10-16 选定）。缎带用飞书图标的蓝 `#3370FF` 一系的轻微渐变，底板沿对角线从 `#FBFBFB` 渐变到左下角的 `#E6EAF0`，比例与渐变参照 DeepSeek 的图标：四周各留约 3%，圆角约为边长的 22%。

- **设计理由**：只用一个隐喻，形状粗壮，细节以挖空表现，不用细线，因为细线在小尺寸下会失去质感，这是此前几稿被否定的原因。Microsoft 的[应用图标设计指南](https://learn.microsoft.com/en-us/windows/apps/design/iconography/app-icon-design)推荐透明背景，作者选择在 Windows 上同样使用底板。底板在深色任务栏上醒目，在浅色任务栏上靠一像素极淡的边勾出轮廓。
- **源稿**：48 格的主稿为 `helloutau/lib/Editor/Resources/icons/brand/app.svg`。16、20、24 与 32 像素各有一张对齐像素网格的小稿，因为 Microsoft 要求小尺寸单独绘制，而不由大图缩小。小稿不纳入仓库，仓库只提交由主稿与小稿生成的 `helloutau/tools/driver/helloutau.ico`（作者 2026-10-16 决定）。32 像素以下底板铺满画布，缎带为纯色，16 像素的控制点各为一个像素。缎带的曲线在两端放缓，以免粗线内侧的半径小于线宽的一半，在衔接处出现折角。
- **`helloutau.ico`** 共 14 帧：16、20、24、30、32、36、40、48、60、64、72、80、96 与 256，即[应用图标构造](https://learn.microsoft.com/en-us/windows/apps/design/iconography/app-icon-construction)一页中标题栏、任务栏与开始菜单在各缩放比例下所用尺寸的并集。30 像素用 24 像素的小稿，36 像素起用主稿。各帧由 QtSvg 渲染，256 像素帧以 PNG 压缩，其余帧为 32 位 BGRA 位图。qmsetup 的 `qm_add_win_rc()` 把图标列为资源脚本的依赖与配置依赖，换了图标后 Ninja、Makefile 与 Visual Studio 都会重新编译资源。
- macOS 需要另做一个满幅方形、由系统加圆角的版本，Linux 安装 `helloutau.svg` 到 `hicolor/scalable/apps`，两者在补充相应平台的打包时再做。

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

设置的「Updates」页位于 Appearance & Behavior 的 System Settings 之下，同 JetBrains IDE，提供以下设置：

- 更新源：「自动」与更新配置中各源的名字，按配置的顺序列出，默认为「自动」。「自动」按系统时区所在的地区（`QTimeZone::systemTimeZone().territory()`）选择，规则见「更新配置」。以现在的配置，Asia/Shanghai 与 Asia/Urumqi 属于 `QLocale::China`，选择 Gitee。香港、澳门、台湾的时区各有自己的地区，选择 GitHub。Windows 的「China Standard Time」由 Qt 映射到 Asia/Shanghai。设置记录源的名字（`updates/source`），自动时不记录。记录的名字在配置中已不存在时，按自动处理。
- 更新方式（作者 2026-10-11 决定）：
  - 「手动」：启动时不检查，只从帮助菜单的「Check for Updates...」检查。
  - 「检查并提示」（默认）：启动时检查，每天至多一次，发现新版本时提示，由用户选择下载、跳过此版本或稍后提醒。
  - 「自动更新」：启动时同样检查，发现新版本即在后台下载并校验，不提示。程序正常退出时静默安装，下次启动即为新版本，同 VS Code 的 update on restart。
  - 任何一种方式都不强制更新。手动检查总是提示，由用户决定，自动更新的设置不作用于手动检查。
- 跳过的版本：用户在提示中选择「跳过此版本」后，启动时不再提示该版本，手动检查仍然提示。

该页的「Check Now」按页上所选的源检查，无论该选择是否已应用。上次成功检查的时间与跳过的版本同样记入设置（`updates/lastCheck`、`updates/skippedVersion`）。

### 更新配置

更新源与附件、安装包的标识不写在代码中，而记录在 `share/helloutau/update/UpdateManifest.json`（作者 2026-10-11 决定，与 Qt 的翻译同样放在 `share` 下）。`share/helloutau` 下只放按内容分类的子文件夹，以后的翻译、外部工具等与 `update` 并列，参照 Qt Creator 的 `share/qtcreator`（作者 2026-10-12 决定）。源文件为 `helloutau/share/helloutau/update/UpdateManifest.json`，构建时复制到构建目录的同一相对位置，安装时装到安装目录的同一位置，程序按 `bin` 的上级目录查找。

代码中不出现任何托管平台的名字或域名，更新源是配置中的一个列表（作者 2026-10-11 决定）。以后增加其他托管平台时，只需在列表中加一项。

| 键 | 含义 | 现值 |
|---|---|---|
| `assetPrefix` | 附件文件名的开头，见「发布物」 | `helloutau` |
| `innoSetupAppId` | 安装包的 `AppId`，必须与 `setup.iss` 相同 | `org.openvpi.helloutau` |
| `sources` | 更新源的列表，至少一项，顺序即设置页中的顺序 | GitHub、Gitee |

`sources` 的每一项：

| 键 | 含义 |
|---|---|
| `name` | 设置页中显示并记入设置的名字，不翻译，不可重复 |
| `latestRelease` | 最新 release 的接口地址，必须是 HTTPS，原样使用，不展开变量 |
| `releasePage` | 可选。release 页面的模板，`${tag}` 替换为百分号编码后的 tag（`stdc::str::varexp`，作者 2026-10-11 建议）。只在接口的响应没有 `html_url` 时使用，例如 Gitee |
| `territories` | 可选。ISO 3166 地区代码的列表（`QLocale::codeToTerritory`），未知的代码使整个文件无效 |

「自动」选择 `territories` 包含系统时区所在地区的第一个源，都不包含时选择列表中的第一个源。

接口的响应须为 GitHub 的 release 格式：`tag_name`、`name`、`body`、`draft`、`prerelease`、`html_url`（可缺），以及 `assets` 中各项的 `name` 与 `browser_download_url`。GitHub 与 Gitee 都采用这一格式，Gitea、Forgejo 等兼容 GitHub 接口的平台同样适用。GitLab 的格式不同，需要支持时再为源增加表示格式的键。请求只带 User-Agent（GitHub 拒绝没有 User-Agent 的请求），不带任何平台专用的请求头。

文件缺失、缺少任一项或任一项无效时，启动时的检查只记入日志，手动检查报告原因。triplet 不在此文件中，因为它描述构建本身，由 CMake 写入 `hellokit/Config.h` 的 `HELLOKIT_TRIPLET`。版本（`HELLOKIT_VERSION`）与构建信息（`hellokit/BuildInfo.h`）同样由 hellokit 生成，因为 hellokit 的 `hello-updater` 与 helloutau 的 `hello-plugin-play` 也要读取。应用名与组织名留在 `helloutau/Config.h`（作者 2026-10-15 定）。

### 检查与下载

1. 以所选源的 `latestRelease` 取得最新的 release。公开仓库无须令牌。
2. 去掉 tag 的 `v` 前缀作为版本号，与 `HELLO_VERSION` 逐段比较，不以 `v` 开头的 tag 不视为版本，较新且附件齐全（见「发布物」）时提示，显示 release 的说明。`v` 之后只接受数字与点，因为 `QVersionNumber::fromString()` 接受段首的空格与正号（实测 `v 0.1`、`v+0.1` 均被接受）。响应没有 `html_url` 时按源的 `releasePage` 构造页面地址。Gitee 的附件列表中还有 Gitee 生成的源码压缩包，按文件名挑选时不受影响。草稿与预发布不视为 release。
3. 用户确认后（「自动更新」时不询问）下载 `SHA256SUMS` 与附件，手动下载时显示进度。只接受 HTTPS，拒绝重定向到 HTTP。附件下载到临时文件总目录下的 `updates`（`UpdateChecker::downloadDirectory()`），而不是会话目录，因为会话目录在程序退出时删除，早于退出时的安装。附件先写入同目录的临时文件（`QSaveFile`），完整后才改名，`updates` 中已有摘要相符的同名文件时不再下载。附件大小上限为 1 GiB，`SHA256SUMS` 为 1 MiB。
4. 以 `SHA256SUMS` 校验附件，不符时删除并报告，不安装。

### 更新对话框

更新的全过程由一个任务（`UpdateTask`）进行，任务在后台运行，与窗口和对话框无关（作者 2026-10-11 决定）：

- 只有一个更新对话框（`UpdateDialog`），是模态的，由「Check for Updates...」、设置页的「Check Now」与启动时发现新版本（「检查并提示」）打开。对话框中嵌入显示任务的控件（`UpdateTaskView`），按任务的阶段显示说明、更新说明、进度与该阶段的按钮。关闭对话框不影响任务，再次打开时显示任务当前的状态。任务没有可显示的结果时（尚未检查、已是最新、检查失败），打开对话框即重新检查。
- 下载、校验与解压对用户是一步，进度显示在同一个进度条上，可以取消，也可以「Run in Background」关闭对话框而任务继续。取消即全部舍弃：中止下载并删除临时文件与已下载的附件，删除解压出的 `<安装目录>.new`。
- 准备完成后提供「Install Now」「Install on Quit」与「Discard Update」。「Install Now」先关闭所有窗口（用户可在其中取消），再安排安装并退出。
- 对话框下方列出程序退出时要启动的程序：已安排的安装与已安排的重启，每一项都可以撤销。
- 「自动更新」不打开对话框，在后台检查、准备，准备完成后安排在退出时安装。用户可以随时打开对话框查看或撤销。
- 以后加入窗口右下角的浮动提示（类似 JetBrains IDE 与 VS Code），显示消息与任务的进度，并从那里打开对话框。

### 安装版与免安装版的区分

若存在 `Software\Microsoft\Windows\CurrentVersion\Uninstall\<AppId>_is1`（先查 HKCU，再查 HKLM），且其 `InstallLocation` 与程序的安装目录相同，则为安装版，否则为免安装版。安装版下载安装包，免安装版下载 zip。判断由 `HelloKitUpdate` 的 `kit::InnoSetup`（`hellokit/Update/Windows/`，只在 Windows 上编译）完成，以 `std::filesystem::equivalent` 比较目录，因为记录的目录以分隔符结尾，大小写也可能不同。其他系统上一律视为免安装版。

### 安装版的更新

更新即以静默方式运行新版本的安装包，由 Inno Setup 完成文件替换与注册表的更新：

1. 程序按退出时的流程逐个关闭窗口，询问是否保存未保存的工程与音源。用户取消时不更新。
2. 程序以 `/SILENT /SUPPRESSMSGBOXES /NORESTART /NOCLOSEAPPLICATIONS /UPDATE /RELAUNCH` 启动安装包，然后退出。`/SILENT` 显示进度而不显示向导。`/UPDATE` 与 `/RELAUNCH` 是本程序的自定义参数，安装包在 `[Code]` 中遍历 `ParamStr()` 检查它们。
3. 安装包在 `InitializeSetup` 中等待程序的互斥量消失，最多等待一定时间，超时则退出并返回失败。程序启动时创建该互斥量。不使用 Inno Setup 的 `CloseApplications` 强制关闭程序，因为程序已在第 1 步正常退出，强制关闭会丢失未保存的修改。
4. 由于沿用上一次的安装目录、任务与安装模式，安装包覆盖原目录的文件。「为所有用户安装」的安装包需要管理员权限，Windows 照常弹出 UAC 提示。
5. Inno Setup 删除旧的卸载信息键并以新的值重建，`DisplayVersion` 随之更新为新版本。文件关联、快捷方式按所选任务重新写入，路径仍指向原安装目录。
6. 安装结束后，`[Run]` 中一项仅在带有 `/RELAUNCH` 时执行的条目重新启动程序。普通的「安装后运行」条目带 `skipifsilent`，静默安装时不执行。
7. 安装包的退出码不为 0 时（例如 2 为用户取消，5 为安装中取消或中止），下次启动的程序不会是新版本。程序在下次启动时比较版本，未更新时提示更新失败，并提供下载页链接。

以上是用户在提示中选择更新时的流程。「自动更新」时，第 1 步即用户正常退出程序的流程，用户已在其中处理未保存的修改。第 2 步在程序退出前启动安装包，以 `/VERYSILENT` 代替 `/SILENT`，不带 `/RELAUNCH`，安装结束后不启动程序。用户在退出时取消关闭窗口则不安装，下载的安装包留待下次退出。需要 UAC 提升的安装（「为所有用户安装」）在退出时仍会弹出 UAC 提示，用户拒绝时下次启动提示更新失败。

实现上的约定：

- 互斥量以安装包的 `AppId` 命名，即 `org.openvpi.helloutau`。`setup.iss` 以 `#define AppId` 定义它，程序取自更新配置的 `innoSetupAppId`，不另设名字。程序启动后以 `kit::InnoSetup::holdMutex()` 创建互斥量，句柄从不关闭，由系统在进程结束时释放，因此安装包等到互斥量消失时，程序的文件已不再被占用。安装包最多等待 60 秒。
- `setup.iss` 同时以 `AppMutex` 指定该互斥量：用户手动运行安装包或卸载程序时，若程序仍在运行，Inno Setup 提示关闭程序，静默安装（`/SUPPRESSMSGBOXES` 默认选「取消」）则中止并返回 1，不会在文件被占用时出错。实测（2026-10-11，Inno Setup 7）`AppMutex` 的检查在 `InitializeSetup` 返回之后进行，因此带 `/UPDATE` 时先完成等待，更新不会遇到该提示。
- 安装包的参数由 `UpdateChecker::installerArguments()` 给出。重新启动程序的 `[Run]` 条目带 `runasoriginaluser`，因此「为所有用户安装」时程序仍以用户身份而不是管理员身份启动。
- 准备完成后，更新对话框提供「立即安装」即上述流程，「退出时安装」与「自动更新」相同，见「更新对话框」。
- 安装包在事件循环结束后由 `kit::ExitLauncher` 启动。该实例由 `AppLoader` 持有，按键名安排退出后启动的程序，重启（`ExitLauncher::scheduleRestart()`）与安装包（键名 `update`）各占一项，同一键名再次安排时替换旧的一项。带 `/RELAUNCH` 的安装会移除已安排的重启，因为安装包本身会重新启动程序。
- 程序在退出时记下要安装的版本与 release 页面（`updates/pending`）。下次启动时无论成败都删除 `updates` 目录中下载的文件，版本未达到该版本则提示安装失败，并可打开 release 页面。程序崩溃时不会记下，因为安装包也不会启动。

Windows 特有的注册表信息因此全部由安装包维护，程序自身不写注册表。

### 免安装版的更新

正在运行的程序文件无法被覆盖，因此由一个独立的更新程序在主程序退出后替换安装目录。解压由主程序完成，更新程序只负责改名与启动（作者 2026-10-11 决定）：解压代码只有一份，与将来安装音源、插件压缩包的代码共用。解压失败在程序仍在运行时即可报告。更新程序极小，出错的余地也小。

1. 程序检查安装目录的上级目录是否可写。不可写时（例如解压到了 `Program Files`）不自动更新，改为打开下载页。
2. 下载并校验 zip 后，程序在后台把它解压到安装目录旁的 `<安装目录>.new`。该目录必须与安装目录同级，因为只有同一卷上的改名是瞬间完成的。已有的 `<安装目录>.new` 先删除。解压遵守 `CLAUDE.md`「安全底线」第二条：拒绝绝对路径、盘符、`..` 路径分量与符号链接条目（拒绝整个压缩包而不是跳过条目），以 `weakly_canonical` 确认目标位于目标目录之内，并设大小与条目数上限。发布的 zip 中有一层顶层目录，所有条目都位于唯一的顶层目录下时去掉这一层。解压失败时报告原因，不安装。
3. 用户选择安装时（或「自动更新」解压完成后），程序把更新程序从 `bin` 复制到临时文件总目录下的 `updates`，并在退出时（`kit::ExitLauncher`）从那里启动它，因为安装目录将被整体替换，更新程序不能运行在其中，也不能加载其中的动态库。更新程序只依赖标准库与系统接口，并静态链接运行库（作者 2026-10-11 决定）：MSVC 的 ABI 用 `/MT`，MinGW 用 `-static`，Linux 的 GCC 用 `-static-libgcc -static-libstdc++`（glibc 不宜完全静态链接），macOS 的运行库属于系统。因此只需复制这一个文件。参数为：`--pid` 等待退出的进程号，`--target` 安装目录，`--source` `<安装目录>.new`，`--launch` 完成后启动的程序（`--` 之后的参数传给它），`--log` 日志文件，`--show-errors`。它在 Windows 上是 GUI 程序，不弹出控制台窗口，`--help` 以消息框显示用法（作者 2026-10-11 决定）。
4. 更新程序等待主程序退出，把安装目录改名为 `<安装目录>.old`，再把 `<安装目录>.new` 改名为安装目录，启动新版本（如有 `--launch`），最后删除 `<安装目录>.old`。改名失败时在 10 秒内重试，因为杀毒软件或资源管理器会短暂打开目录中的文件。仍然失败时恢复原安装目录，并照常启动程序，此时启动的是旧版本。
   - 更新程序不显示进度，两次改名是瞬间完成的。错误写入日志，下次启动的程序在删除 `updates` 之前读取日志，报告更新失败的原因。唯一的例外是恢复也失败：安装目录已不存在，程序无法启动，因此带 `--show-errors` 时更新程序以消息框告知旧版本所在的目录（作者 2026-10-11 决定）。消息框由静态库 `HelloKitNativeSystem` 提供，不增加动态库的依赖：Windows 为任务对话框 `TaskDialogIndirect()`，程序未使用版本 6 的 Common Controls 或任务对话框失败时为 `MessageBoxW()`，macOS 为 CoreFoundation 的 `CFUserNotificationDisplayAlert()`，Linux 由 zenity 或 kdialog 显示，两者都没有或没有显示器时只写日志（作者 2026-10-16 决定）。
   - 参数无效时，带 `--show-errors` 则以消息框显示用法，否则写到标准错误，以免测试被消息框阻塞。
   - `--help` 显示用法，`--version` 显示版本（`HELLOKIT_VERSION`），退出码为 0。在 Windows 上，标准输出被重定向到文件或管道时写到标准输出，否则以消息框显示（作者 2026-10-15 要求，原先 `--help` 一律弹出消息框）。
5. 「立即安装」时更新程序启动新版本后退出，「退出时安装」与「自动更新」时不启动。
6. 程序在退出时记下要安装的版本（`updates/pending`），下次启动时报告结果，同安装版。
7. 下次启动时，无论更新成败，程序都删除 `updates` 目录（下载的附件、复制出的更新程序与日志）与残留的 `<安装目录>.new`、`<安装目录>.old`，不在临时目录与安装目录旁留下文件（作者 2026-10-11 决定）。删除之前读取更新程序的日志，更新失败时作为提示的详细信息显示。这一步只在记有待安装的版本时进行，因为另一个正在运行的实例可能正在解压 `<安装目录>.new`。用户更新后不再启动程序时，这些文件留在系统临时目录中，由系统清理。

更新程序复制到安装目录之外运行，因为整个安装目录要被改名替换。VS Code 不复制更新程序：它的 Windows zip 版不支持自动更新，安装版的 `inno_updater.exe` 在安装目录内逐个移动文件，新的版本各占一个资源目录。逐个移动文件失去了整体替换与整体还原，因此本程序不采用（作者 2026-10-11 决定）。

安装目录在更新前后路径不变，用户自行创建的快捷方式与文件关联因此仍然有效。免安装版不写注册表，也没有需要更新的注册表信息。

## 模块划分

- `HelloKitUpdate`（子库，`hellokit/Update/`，命名空间 `hello::kit`）：与具体程序无关的更新工具。程序名、仓库地址、triplet 与源的选择规则都由调用方传入。私有依赖 Qt Network，不依赖 QtWidgets。类型名带 `Software` 前缀，因为 `Release` 一类的名字过于宽泛（作者 2026-10-11 决定）。
  - `SoftwareRelease`、`SoftwareReleaseAsset`：release 的内容、GitHub 格式的解析、页面模板的展开、tag 到版本号的转换。
  - `SoftwareReleaseQuery`：按调用方给出的地址取得最新的 release，只接受 HTTPS（`NoLessSafeRedirectPolicy`），使用系统代理，并带有 User-Agent。
  - `Sha256Sums`：读取 `SHA256SUMS` 与计算文件的摘要。
  - `Windows/InnoSetup`：安装记录的查询，见「安装版与免安装版的区分」。
  - `SoftwareReleaseDownload`：下载附件并以 `SHA256SUMS` 校验，见「检查与下载」。只接受不含路径的文件名作为附件名。
- `kit::ZipExtractor`（`HelloKitSupport`，私有依赖 libzip）：安全解压 zip，检查的规则见 `hellokit/Support/ZipExtractor.h`。由主程序使用，将来安装音源、插件压缩包时同样使用。放在 `HelloKitSupport` 中而不另设子库（作者 2026-10-11 决定）。不带 UTF-8 标志的非 ASCII 条目名由调用方提供的解码函数解码，未提供时拒绝整个压缩包。
- 更新程序 `hello-updater`（`hellokit/tools/updater`）：免安装版的目录替换，参数全部来自命令行，不含任何 HelloUtau 特有的内容。它不拆出库，因为它只能依赖标准库与系统接口，库会成为它的依赖。测试在 `tests/auto/tools/updater` 中实际运行它，检查替换与回滚的结果。
- 试听程序 `hello-plugin-play`（`helloutau/tools/plugin-play`）：原版插件所用 playinplugin（Windows 上为 playinplugin.exe）的兼容程序，与 `hello-updater` 一样只依赖标准库与系统接口，安装在 `bin` 中，由设置页复制到插件文件夹，见 [`PluginPlay.md`](PluginPlay.md)。
- `SoftwareTreeUpdate`（helloutau，`HelloUtauEditor`）：免安装版更新中与界面无关的部分。在后台线程把 zip 解压到 `<安装目录>.new` 并检查程序存在，失败或取消时删除 `.new`。生成 `hello-updater` 的参数并把它复制到 `updates`。下次启动时读取更新程序的日志，删除残留的 `.new` 与 `.old`。
- 临时目录管理器：`HelloKitSupport`。
- 解压 zip 使用 libzip（BSD 许可证，vcpkg 有端口，作者 2026-10-11 决定），动态链接，与其他 vcpkg 依赖相同。不采用 Qt 的 `QZipReader`：它是私有接口，不带 UTF-8 标志的条目名按本机代码页解码而取不到原始字节，`extractAll()` 也不检查路径。
  - 以 `ZIP_FL_ENC_RAW` 取得条目名的原始字节。音源的压缩包常以 Shift_JIS 或 GBK 写条目名而不标 UTF-8，按 `CLAUDE.md`「编码」，这些字节必须交给 `winacp` 解码，不能由解压库自行转换。
  - libzip 文档规定不同的 `zip_t` 对象互相独立，可在多个线程中并行使用而无须加锁。多线程解压时每个工作线程各自打开一次压缩包，分别解压不同的条目。
  - 不采用 bit7z：它在运行时需要随程序提供 7-Zip 的 `7z.dll`，条目名由 7-Zip 解码后以宽字符串给出，无法按本仓库的规则自行解码。
- helloutau：`UpdateChecker`（读取更新配置、按名字或时区选择更新源、附件名、启动时与手动的检查，持有任务并打开对话框），`UpdateTask`（检查、下载、校验、解压、安排安装的任务），`UpdateTaskView` 与 `UpdateDialog`（更新对话框），「Updates」设置页，以及以后的关闭窗口与退出的流程。启动时的检查由 Core 插件在打开首批窗口之后发起，只在有 `AppLoader` 时进行，因此测试不发出请求。

## 参考

- **diffscope**：安装与卸载脚本（`dist/installer/windows/setup.iss.in`、`uninstall.iss.in`），本文的安装模式、文件关联与卸载选项均沿用其写法。
- **Visual Studio Code**（同样使用 Inno Setup）：更新时以 `/verysilent`、`/nocloseapplications` 与自定义的 `/update=` 参数运行新的安装包（`src/vs/platform/update/electron-main/updateService.win32.ts`），以 `checksum` 核对下载文件的 SHA-256。它在程序运行期间把新文件安装到 `{app}\_`，程序退出后由 `inno_updater.exe` 替换（`build/win32/code.iss`、`microsoft/inno-updater`），以实现后台更新。HelloUtau 体积小，不需要后台更新，因此只采用前一半：先退出程序，再静默运行安装包。
- **Notepad++ 的 WinGUp**：通用的更新程序，以 XML 告知新版本与下载地址，下载后运行安装包，并以 `WinVerifyTrust` 校验安装包的 Authenticode 签名（`notepad-plus-plus/wingup`）。HelloUtau 的 `HelloKitUpdate` 与更新程序同样与具体程序无关，以后签名时再加入签名校验。
- **Squirrel.Windows**：安装到 `%LocalAppData%\<应用>`，各版本并列于 `app-<版本>` 目录，由 `Update.exe` 启动，按 `RELEASES` 文件在增量包与全量包之间选择。HelloUtau 只做全量更新，不采用这种布局。
