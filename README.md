# HelloUtau

A cross-platform editor for UTAU projects and voice banks, for Windows, macOS and Linux.

HelloUtau aims at functional parity with UTAU 0.4.19, while its interface, its handling of text encodings and its extension mechanism are designed anew.

## Status

Early development. The editor window is not implemented yet. The libraries in `hellokit` are implemented and tested on all three systems:

- Reading and writing of `.ust` and of `.usth`, the native project format
- Voice banks: reading, editing and saving `oto.ini`, `prefix.map` and `character.txt` in the encoding of each directory, and detection of changes on disk
- Rendering with UTAU resamplers and wavtools
- MIDI import and export

## Compatibility

Data and protocols match UTAU: the file formats, the command-line conventions of engines, the temporary files of plugins, and the audio rendered from the same project. The parameters passed to the engines are compared with those of UTAU by the tools in `hellokit/tests/manual`. The interface, the interaction and the internal structure are not bound to UTAU.

Engine paths recorded in a project file are preserved, but never executed without the confirmation of the user.

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
