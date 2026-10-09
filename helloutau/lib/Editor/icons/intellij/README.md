# IntelliJ 图标

本目录的图标取自 [JetBrains/intellij-community](https://github.com/JetBrains/intellij-community) 提交 `c85848e09fe71cff27b7b484596c4043614849eb` 的 `platform/icons/src/expui`，许可为 Apache License 2.0，与本项目相同，许可全文见仓库根目录的 [LICENSE](../../../../../LICENSE)。原文件带有版权注释的，予以保留。

| 文件 | 来源 | 命令 |
|---|---|---|
| `undo.svg` | `general/undo.svg` | 撤销 |
| `redo.svg` | `general/redo.svg` | 重做 |
| `edit.svg` | `general/edit.svg` | 画笔工具 |
| `search.svg` | `general/search.svg` | 查找 |
| `run.svg` | `run/run.svg` | 播放 |
| `pause.svg` | `run/pause.svg` | 播放（播放与渲染中） |
| `stop.svg` | `run/stop.svg` | 停止 |
| `rerun.svg` | `run/rerun.svg` | 重播 |
| `freeze.svg` | `debugger/freeze.svg` | Mode2 转为 Mode1 |
| `runToCursor.svg` | `run/runToCursor.svg` | 从偏移播放到 cutoff |
| `lightning.svg` | `actions/lightning.svg` | 试合成 |

最后三项在 IntelliJ 中没有含义相同的图标，按含义相近选取：雪花取「冻结」之义，即把计算出的曲线固定为数值。IntelliJ 中没有可用图形的命令，图标由本项目绘制，见 [`../helloutau/README.md`](../helloutau/README.md)。

对原文件的修改：

- 灰色 `#6C707E` 改为 `currentColor`，由 `ThemeIcon` 按控件的文字颜色绘制，因此不需要原仓库中深色主题的 `_dark` 文件。
- `run.svg` 与 `stop.svg` 的浅色填充（`#F2FCF3`、`#FFF7F7`）改为 `none`，`rerun.svg` 删去同样的浅色填充路径。这些填充只适用于浅色主题，绿色、红色、橙色与蓝色的描边和图形保留。
- 换行改为 LF。
