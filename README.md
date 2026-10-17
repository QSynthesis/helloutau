# HelloUtau

English | [简体中文](docs/README.zh-CN.md)

A modern, faithfully compatible, cross-platform UTAU editor, with the frequency map editor and the classic tools built in, and a polished user experience.

The goal of HelloUtau is functional parity with UTAU 0.4.19. Its interface, its handling of text encodings and its extension mechanism are designed anew.

## Installation

Each release on [GitHub](https://github.com/QSynthesis/helloutau/releases) and [Gitee](https://gitee.com/functioner/helloutau/releases) provides two packages for 64-bit Windows 10 and later:

| Package | File |
|---|---|
| Installer | `helloutau-<version>-x86_64-windows-msvc-setup.exe` |
| Portable package, extracted into any folder | `helloutau-<version>-x86_64-windows-msvc.zip` |

`SHA256SUMS` lists the SHA-256 digest of each package. The packages contain every required library, including the C++ runtime. Neither package is signed, so Windows SmartScreen displays a warning on the first start. On macOS and Linux, HelloUtau is built from source, as described in [Building](#building).

By default, the application checks for a new version at startup, at most once a day, and installs a new version only after confirmation. The Updates page of the settings selects the update source and the update mode: manual checks, checks with a notification, or automatic updates.

## First setup

The [user guide](docs/UserGuide.md), in Chinese, describes the use of the editor. The first setup consists of three steps:

1. Set the UTAU folder, in which voice banks and plugins are located.
2. Set the default wavtool and resampler on the Audio Rendering page of the settings.
3. Choose a playback mode, and configure the render log if required.

## Features

- Projects: reading and writing of `.ust` and of `.usth`, the native project format. The piano roll edits notes, lyrics, labels, regions, Mode1 and Mode2 pitch, vibrato, envelopes and note parameters, with undo and redo, lyric search and replacement, the project tempo, the time signature, quantization and a piano preview of the keys.
- Playback and rendering: classic or multithreaded prerendering, realtime playback, export of the whole track as WAV, render logs, a diagnosis of failed renders, and trust checks for the synth tools of a project.
- Voice banks: a window that edits `oto.ini`, `prefix.map`, `character.txt` and `readme.txt` in the encoding of each folder, sets the values of an entry on its waveform, previews samples and detects changes on disk. Misdecoded file names are read as UTAU reads them, or converted together with the text encodings.
- Frequency maps: the frequency map curve on the waveform, in the format of the configured resampler by default, a spectrogram view, and the generation, regeneration and removal of the maps of a voice bank with the resampler. The formats frq, dio and mrq are supported, together with the analysis caches of their resamplers.
- Plugins: HelloUtau plugins and UTAU classic plugins, and a command palette that lists the commands of the current window.
- Import and export: MIDI, UST and USTH.
- Workspace: separate shortcuts for project windows and voice bank windows, configurable modifier keys of the project editor, recent projects and voice banks, and updates from GitHub or Gitee.

## Compatibility

HelloUtau reads and writes UTAU projects and voice banks, and runs UTAU resamplers, wavtools and classic plugins. Projects exchanged with UTAU keep their content, and the rendering follows the behavior of UTAU.

- The synth tool paths recorded in a project are preserved.
- The synth tools of a project are executed only if their paths are valid and the user trusts them.
- The synth tools of a project that are the default synth tools of the settings require no confirmation.

## User documentation

The [user guide](docs/UserGuide.md) covers the first setup, UTAU compatibility, note and pitch editing, selection, playback, render logs, voice banks, frequency maps, plugins, shortcuts and the modifier keys of the editor.

## Building

Requirements:

- CMake 3.19 or later and a C++17 compiler. MSVC 2022, GCC 11 and Apple Clang 14 are tested.
- Qt 6. Qt 6.10 and 6.11 are tested.
- The following packages, each built and installed separately, or installed with the ports of [stdware/vcpkg-overlay](https://github.com/stdware/vcpkg-overlay) as the workflow does. Each is found through `CMAKE_PREFIX_PATH` or the toolchain of vcpkg, or otherwise at the directory that its `<name>_DIR` variable names:

| Package | Source | Variable |
|---|---|---|
| QActionKit | [stdware/qactionkit](https://github.com/stdware/qactionkit), branch `next` | `QActionKit_DIR=<prefix>/lib/cmake/QActionKit` |
| qmsetup | [stdware/qmsetup](https://github.com/stdware/qmsetup) | `qmsetup_DIR=<prefix>/lib/cmake/qmsetup` |
| stdcorelib | [stdware/stdcorelib](https://github.com/stdware/stdcorelib) | `stdcorelib_DIR=<prefix>/lib/cmake/stdcorelib` |
| stdcorelib.plugin | [stdware/stdcorelib.plugin](https://github.com/stdware/stdcorelib.plugin), a shared library | `stdcorelib-plugin_DIR=<prefix>/lib/cmake/stdcorelib-plugin` |
| stdutau | [diffscope/stdutau](https://github.com/diffscope/stdutau) | `stdutau_DIR=<prefix>/lib/cmake/stdutau` |
| substate | [stdware/substate](https://github.com/stdware/substate) | `substate_DIR=<prefix>/lib/cmake/substate` |
| winacp | [QSynthesis/winacp](https://github.com/QSynthesis/winacp) | `winacp_DIR=<prefix>/lib/cmake/winacp` |
| wolf-midi | [wolfgitpr/wolf-midi](https://github.com/wolfgitpr/wolf-midi) | `wolf-midi_DIR=<prefix>/lib/cmake/wolf-midi` |
| r8brain-free-src 6.5 | [avaneev/r8brain-free-src](https://github.com/avaneev/r8brain-free-src), built by the vcpkg port in [diffscope/diffscope-project](https://github.com/diffscope/diffscope-project) (`scripts/vcpkg/ports/r8brain-free-src`) | `unofficial-r8brain-free-src_DIR=<prefix>/share/unofficial-r8brain-free-src` |
| libzip | [nih-at/libzip](https://github.com/nih-at/libzip), for example from vcpkg | `libzip_DIR=<prefix>/share/libzip` |
| zlib (Windows only) | The zlib against which libzip was built, whose DLL is copied beside the application | `ZLIB_DIR=<prefix>/share/zlib` |

The application also requires the Qt Multimedia and Qt SVG modules.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    "-DCMAKE_PREFIX_PATH=<Qt>;<prefix of the packages>"
cmake --build build
ctest --test-dir build
```

## Repository layout

| Directory | Content |
|---|---|
| `hellokit` | Libraries without a graphical interface, their tests, and the file system monitor `hello-fswatcher` |
| `helloutau` | The editor application, based on Qt Widgets |
| `docs` | Design documents, in Chinese |

## License

Apache License 2.0. See [LICENSE](LICENSE).
