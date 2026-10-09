# HelloUtau

English | [简体中文](docs/README.zh-CN.md)

A cross-platform editor for UTAU projects and voice banks, for Windows, macOS and Linux.

HelloUtau aims at functional parity with UTAU 0.4.19, while its interface, its handling of text encodings and its extension mechanism are designed anew.

For the end-user workflow, see the [user guide](docs/UserGuide.md). To get started:

1. Set the UTAU folder so that voice banks and plugins can be located.
2. Set the default wavtool and resampler in Audio Rendering.
3. Choose a playback mode and configure render logs as needed.

## Status

The editor offers a comprehensive set of features for UTAU project editing, rendering, and voice-bank editing. Compatibility and usability continue to improve. Features include:

- Projects: reading and writing of `.ust` and `.usth`, the native project format. A piano roll edits notes, lyrics, labels, regions, Mode1 and Mode2 pitch, pitch control, vibrato, envelopes and note parameters, with undo and redo, lyric find and replace, project tempo and time-signature views, quantization, and piano-like keyboard preview.
- Playback and rendering: classic or multithreaded prerendering, realtime playback, whole-track WAV export, configurable render logs, and trust checks for project synth tools
- Voice banks: a window that edits `oto.ini`, `prefix.map`, `character.txt` and `readme.txt` in the encoding of each directory, sets the values of an entry on its waveform, previews samples, displays the current voice-bank image above the piano keyboard, and detects changes on disk
- Plugins: HelloUtau plugins and UTAU classic plugins, with command discovery through the command palette
- Import and export: MIDI, UST, and USTH
- Workspace: separate command settings for project and voice-bank windows, configurable project editor modifiers, recent projects and voice banks, and separate voice-bank encoding choices

## Compatibility

HelloUtau supports UTAU projects, voice banks, resamplers, wavtools, and classic plugins. It aims to preserve project content and rendering behavior when exchanging projects with UTAU, while offering its own editing interface.

- Synth Tool paths recorded in a project file are preserved.
- The application checks that paths are valid and asks for trust before running project-specific tools.
- Project tools pointing to the same files as the default tools are considered trusted.

## User documentation

The [user guide](docs/UserGuide.md) covers setup, UTAU compatibility, note and pitch editing, selection, playback, rendering logs, voice banks, plugins, shortcuts, and configurable editor modifiers.

## Building

Requirements:

- CMake 3.19 or later and a C++17 compiler. MSVC 2022, GCC 11 and Apple Clang 14 are tested.
- Qt 6. Qt 6.10 and 6.11 are tested.
- The following packages, each built and installed separately and located through its `<name>_DIR` variable:

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

The application also requires the Qt Multimedia and Qt SVG modules.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=<Qt> \
    -DQActionKit_DIR=... -Dqmsetup_DIR=... -Dstdcorelib_DIR=... -Dstdcorelib-plugin_DIR=... \
    -Dstdutau_DIR=... -Dsubstate_DIR=... -Dwinacp_DIR=... -Dwolf-midi_DIR=... \
    -Dunofficial-r8brain-free-src_DIR=...
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
