# 本项目绘制的图标

IntelliJ 图标库中没有可用图形的命令，图标按作者的描述绘制（2026-10-01），尺寸、线宽与端点沿用 [`../intellij/`](../intellij/README.md) 的 expui 图标：16×16，1 像素描边，圆形端点，颜色为 `currentColor`。

| 文件 | 命令 | 图形 |
|---|---|---|
| `selectTool.svg` | 选择工具 | 鼠标指针 |
| `pitchTool.svg` | 手绘音高工具 | 一支笔与一条起伏的线。笔是 `general/edit.svg` 的路径缩小为 0.62 倍，许可同 `../intellij/` |
| `mode2.svg` | Mode2 | 半个周期的正弦线（自 −1 至 1），两端各一个空心的控制点 |
| `showPitch.svg` | 显示音高 | 音符条上方一条起伏的线 |
| `showEnvelopes.svg` | 显示包络 | 音符条上方包络的轮廓 |
| `showParameters.svg` | 显示参数 | 音符条下方两行文字 |
| `crossfadeP2P3.svg` | 包络淡化（p2、p3） | 上方为数字 23，下方为包络的轮廓 |
| `crossfadeP1P4.svg` | 包络淡化（p1、p4） | 上方为数字 14，下方为包络的轮廓 |

三个显示开关共用一根音符条，所显示的内容画在 UTAU 卷帘中相应的位置：音高与包络在音符之上，参数在音符之下。
