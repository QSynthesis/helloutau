# HelloUtau

[English](../README.md) | 简体中文

HelloUtau 是 UTAU 工程与音源的跨平台编辑器，支持 Windows、macOS 与 Linux。

HelloUtau 的目标是在功能上与 UTAU 0.4.19 对等，界面、文本编码的处理与扩展机制则重新设计。

## 状态

处于早期开发阶段。编辑器已可使用，但功能尚不完整，界面仍在评审中。`hellokit` 中的库已在三个系统上通过测试。已实现的内容：

- 工程：`.ust` 与本项目格式 `.usth` 的读写。卷帘可编辑音符、歌词、Mode1 与 Mode2 的音高曲线、颤音、包络与音符参数，支持撤销、重做以及歌词的查找与替换。
- 播放：经 UTAU 的渲染脚本或以多线程进行的预渲染、实时播放，以及将整轨渲染为 WAV 文件
- 音源：音源窗口按各目录的编码编辑 `oto.ini`、`prefix.map`、`character.txt` 与 `readme.txt`，在波形上设定条目的数值，试听样本，并检测磁盘上的变化
- 插件：HelloUtau 的原生插件与 UTAU 的插件
- 导入与导出：MIDI

## 兼容性

数据与协议与 UTAU 一致，包括文件格式、引擎的命令行约定、插件的临时文件，以及同一工程渲染出的音频。传给引擎的参数由 `hellokit/tests/manual` 中的工具与 UTAU 的参数逐项比较。界面、交互与内部结构不受 UTAU 的约束。

工程文件中记录的引擎路径予以保留，但未经用户确认不会执行。

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
