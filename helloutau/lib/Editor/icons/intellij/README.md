# IntelliJ 图标

本目录的图标取自 [JetBrains/intellij-community](https://github.com/JetBrains/intellij-community) 提交 `c85848e09fe71cff27b7b484596c4043614849eb` 的 `platform/icons/src/expui`，许可为 Apache License 2.0，与本项目相同，许可全文见仓库根目录的 [LICENSE](../../../../../LICENSE)。每个文件保留原有的版权注释。

| 文件 | 来源 |
|---|---|
| `undo.svg` | `general/undo.svg` |
| `redo.svg` | `general/redo.svg` |
| `edit.svg` | `general/edit.svg` |
| `search.svg` | `general/search.svg` |
| `run.svg` | `run/run.svg` |
| `pause.svg` | `run/pause.svg` |
| `stop.svg` | `run/stop.svg` |
| `rerun.svg` | `run/rerun.svg` |
| `inSelection.svg` | `actions/inSelection.svg` |
| `palette.svg` | `toolwindows/palette.svg` |
| `graphLayout.svg` | `graph/graphLayout.svg` |
| `softWrap.svg` | `general/softWrap.svg` |
| `coverage.svg` | `toolwindows/coverage.svg` |
| `parameter.svg` | `nodes/parameter.svg` |
| `merge.svg` | `vcs/merge.svg` |
| `arrowLeftRight.svg` | `diff/arrowLeftRight.svg` |
| `runToCursor.svg` | `run/runToCursor.svg` |
| `lightning.svg` | `actions/lightning.svg` |

后十个用于 IntelliJ 中没有对应图标的命令，按形状相近选取，作者 2026-10-01 同意暂用，以后可能更换：选择工具、手绘音高、Mode2、显示音高、显示包络、显示参数、两种包络淡化、从偏移播放到 cutoff、试合成，次序同上表。

对原文件的修改：

- 灰色 `#6C707E` 改为 `currentColor`，由 `ThemeIcon` 按控件的文字颜色绘制，因此不需要原仓库中深色主题的 `_dark` 文件。
- `run.svg`、`stop.svg` 与 `parameter.svg` 的浅色填充（`#F2FCF3`、`#FFF7F7`、`#FFF4EB`）改为 `none`，`rerun.svg` 删去同样的浅色填充路径。这些填充只适用于浅色主题，绿色、红色、橙色与蓝色的描边和图形保留。
- 换行改为 LF。
