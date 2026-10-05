# HelloUtau

[English](../README.md) | 简体中文

HelloUtau 是 UTAU 工程与音源的跨平台编辑器，支持 Windows、macOS 与 Linux。

HelloUtau 的目标是在功能上与 UTAU 0.4.19 对等，界面、文本编码的处理与扩展机制则重新设计。

面向用户的操作说明见《[用户指南](UserGuide.md)》。首次使用时：

1. 设置 UTAU 文件夹，以便查找音源和插件。
2. 在“音频渲染”设置中指定默认合成器和重采样器。
3. 根据需要选择播放方式和渲染日志设置。

## 状态

编辑器已具备较完善的 UTAU 工程编辑、渲染和音源编辑功能，兼容性与使用体验仍在持续改进。主要功能：

- 工程：`.ust` 与本项目格式 `.usth` 的读写。卷帘可编辑音符、歌词、标签、区间、Mode1 与 Mode2 的音高、音高控制、颤音、包络与音符参数，支持撤销、重做、歌词的查找与替换、工程曲速、拍号视图、量化，以及钢琴音色试听。
- 播放与渲染：经典预渲染、多线程预渲染、实时播放、整轨 WAV 导出、渲染日志和工程工具信任检查
- 音源：音源窗口按各目录的编码编辑 `oto.ini`、`prefix.map`、`character.txt` 与 `readme.txt`，在波形上设定条目的数值，试听样本，在钢琴键上方显示当前音源头像，并检测磁盘上的变化
- 插件：HelloUtau 插件与 UTAU 经典插件，可通过命令面板查找命令
- 导入与导出：MIDI、UST 与 USTH
- 工作区：工程窗口和音源窗口分别配置命令，工程窗口支持自定义编辑修饰键，支持最近工程、最近音源和按目录选择音源编码

## 兼容性

HelloUtau 支持 UTAU 工程、音源、重采样器、合成器和经典插件，在提供自身编辑界面的同时，尽可能保持与 UTAU 交换工程时的内容和渲染行为一致。

- 工程文件中记录的工具路径予以保留。
- 程序会检查路径是否有效，并在运行工程工具前请求信任。
- 工程使用与默认工具相同的文件时，视为已信任。

## 用户文档

《[用户指南](UserGuide.md)》覆盖首次设置、UTAU 兼容性、音符和音高编辑、选择、播放、渲染日志、音源、插件、快捷键以及编辑器修饰键。

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

应用程序另需 Qt Multimedia 与 Qt SVG 模块。

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=<Qt> \
    -DQActionKit_DIR=... -Dqmsetup_DIR=... -Dstdcorelib_DIR=... -Dstdcorelib-plugin_DIR=... \
    -Dstdutau_DIR=... -Dsubstate_DIR=... -Dwinacp_DIR=... -Dwolf-midi_DIR=... \
    -Dunofficial-r8brain-free-src_DIR=...
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
