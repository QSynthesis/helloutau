# HelloUtau

[English](../README.md) | 简体中文

HelloUtau 是 UTAU 工程与音源的跨平台编辑器，支持 Windows、macOS 与 Linux。

HelloUtau 的目标是在功能上与 UTAU 0.4.19 一致，界面、文本编码的处理与扩展机制重新设计。

## 安装

[GitHub](https://github.com/QSynthesis/helloutau/releases) 与 [Gitee](https://gitee.com/functioner/helloutau/releases) 上的每个版本提供两种适用于 64 位 Windows 10 及以上版本的发布包：

| 发布包 | 文件 |
|---|---|
| 安装包 | `helloutau-<版本>-x64-windows-msvc-setup.exe` |
| 免安装版，解压到任意文件夹即可使用 | `helloutau-<版本>-x64-windows-msvc.zip` |

`SHA256SUMS` 列出各发布包的 SHA-256 摘要。发布包包含所需的全部库，包括 C++ 运行库。发布包未经签名，首次启动时 Windows SmartScreen 会显示警告。在 macOS 与 Linux 上须从源代码构建，见「构建」一节。

程序默认在启动时检查新版本，每天至多一次，新版本经用户确认后才安装。设置中的「更新」页用于选择更新源与更新方式：手动检查、检查并提示，或自动更新。

## 首次设置

《[用户指南](UserGuide.md)》说明编辑器的使用方法。首次设置分为三步：

1. 设置 UTAU 文件夹，音源与插件在该文件夹中查找。
2. 在设置的「音频渲染」页中指定默认的合成器与重采样器。
3. 选择播放方式，并按需要设置渲染日志。

## 功能

- 工程：`.ust` 与本项目格式 `.usth` 的读写。卷帘编辑音符、歌词、标签、区间、Mode1 与 Mode2 音高、颤音、包络与音符参数，并提供撤销与重做、歌词的查找与替换、工程曲速、拍号、量化，以及钢琴键的试听。
- 播放与渲染：经典预渲染、多线程预渲染、实时播放、整轨导出为 WAV、渲染日志、渲染失败的诊断，以及工程合成工具的信任检查。
- 音源：音源窗口按各文件夹的编码编辑 `oto.ini`、`prefix.map`、`character.txt` 与 `readme.txt`，在波形上设定条目的数值，试听样本，并检测磁盘上的变化。按错误的代码页解码的文件名可以与 UTAU 一样按原样读取，也可以连同文本编码一并转换。
- 频率表：在波形上显示频率表曲线，默认采用设置中的重采样器对应的格式，提供频谱图视图，并以重采样器生成、重新生成与删除音源的频率表。支持 frq、dio 与 mrq 三种格式及其重采样器的分析缓存。
- 插件：HelloUtau 插件与 UTAU 经典插件，命令面板列出当前窗口的命令。
- 导入与导出：MIDI、UST 与 USTH。
- 工作区：工程窗口与音源窗口分别设置快捷键，工程编辑器的修饰键可以自定义，最近的工程与音源，以及从 GitHub 或 Gitee 更新。

## 兼容性

HelloUtau 读写 UTAU 的工程与音源，并运行 UTAU 的重采样器、合成器与经典插件。与 UTAU 交换的工程保持原有的内容，渲染遵循 UTAU 的行为。

- 工程中记录的合成工具路径原样保留。
- 工程的合成工具只在路径有效且经用户信任时才运行。
- 与设置中的默认合成工具相同的工程合成工具不需要确认。

## 用户文档

《[用户指南](UserGuide.md)》涵盖首次设置、UTAU 兼容性、音符与音高的编辑、选择、播放、渲染日志、音源、频率表、插件、快捷键，以及编辑器的修饰键。

## 构建

要求：

- CMake 3.19 或更高版本，以及支持 C++17 的编译器。已测试 MSVC 2022、GCC 11 与 Apple Clang 14。
- Qt 6。已测试 Qt 6.10 与 6.11。
- 下列软件包，各自单独构建并安装，通过对应的 `<名称>_DIR` 变量指定位置：

| 软件包 | 来源 | 变量 |
|---|---|---|
| QActionKit | [stdware/qactionkit](https://github.com/stdware/qactionkit)，`next` 分支 | `QActionKit_DIR=<前缀>/lib/cmake/QActionKit` |
| qmsetup | [stdware/qmsetup](https://github.com/stdware/qmsetup) | `qmsetup_DIR=<前缀>/lib/cmake/qmsetup` |
| stdcorelib | [stdware/stdcorelib](https://github.com/stdware/stdcorelib) | `stdcorelib_DIR=<前缀>/lib/cmake/stdcorelib` |
| stdcorelib.plugin | [stdware/stdcorelib.plugin](https://github.com/stdware/stdcorelib.plugin)，动态库 | `stdcorelib-plugin_DIR=<前缀>/lib/cmake/stdcorelib-plugin` |
| stdutau | [diffscope/stdutau](https://github.com/diffscope/stdutau) | `stdutau_DIR=<前缀>/lib/cmake/stdutau` |
| substate | [stdware/substate](https://github.com/stdware/substate) | `substate_DIR=<前缀>/lib/cmake/substate` |
| winacp | [QSynthesis/winacp](https://github.com/QSynthesis/winacp) | `winacp_DIR=<前缀>/lib/cmake/winacp` |
| wolf-midi | [wolfgitpr/wolf-midi](https://github.com/wolfgitpr/wolf-midi) | `wolf-midi_DIR=<前缀>/lib/cmake/wolf-midi` |
| r8brain-free-src 6.5 | [avaneev/r8brain-free-src](https://github.com/avaneev/r8brain-free-src)，由 [diffscope/diffscope-project](https://github.com/diffscope/diffscope-project) 中的 vcpkg port（`scripts/vcpkg/ports/r8brain-free-src`）构建 | `unofficial-r8brain-free-src_DIR=<前缀>/share/unofficial-r8brain-free-src` |
| libzip | [nih-at/libzip](https://github.com/nih-at/libzip)，例如取自 vcpkg | `libzip_DIR=<前缀>/share/libzip` |
| zlib（仅 Windows） | 构建 libzip 时使用的 zlib，其 DLL 复制到程序旁 | `ZLIB_DIR=<前缀>/share/zlib` |

应用程序另需 Qt Multimedia 与 Qt SVG 模块。

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=<Qt> \
    -DQActionKit_DIR=... -Dqmsetup_DIR=... -Dstdcorelib_DIR=... -Dstdcorelib-plugin_DIR=... \
    -Dstdutau_DIR=... -Dsubstate_DIR=... -Dwinacp_DIR=... -Dwolf-midi_DIR=... \
    -Dunofficial-r8brain-free-src_DIR=... -Dlibzip_DIR=... -DZLIB_DIR=...
cmake --build build
ctest --test-dir build
```

## 仓库结构

| 目录 | 内容 |
|---|---|
| `hellokit` | 不含图形界面的库及其测试，以及文件系统监视程序 `hello-fswatcher` |
| `helloutau` | 基于 Qt Widgets 的编辑器应用程序 |
| `docs` | 设计文档（中文） |

## 许可证

Apache License 2.0，见 [LICENSE](../LICENSE)。
