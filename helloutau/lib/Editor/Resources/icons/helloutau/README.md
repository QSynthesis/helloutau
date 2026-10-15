# 本项目绘制的图标

IntelliJ 图标库中没有可用图形的命令，图标按作者的描述绘制（2026-10-01），尺寸、线宽与端点沿用 [`../intellij/`](../intellij/README.md) 的 expui 图标：16×16，1 像素描边，圆形端点，颜色为 `currentColor`。

| 文件 | 命令 | 图形 |
|---|---|---|
| `selectTool.svg` | 选择工具 | 鼠标指针 |
| `pitchTool.svg` | 手绘音高工具 | 一支笔与一条起伏的线。笔是 `general/edit.svg` 的路径缩小为 0.62 倍，许可同 `../intellij/` |
| `mode2.svg` | Mode2 | 半个周期的正弦线（自 −1 至 1），两端各一个空心的控制点 |
| `showPitch.svg` | 显示音高 | 音符条上方一条起伏的线 |
| `showRenderedPitch.svg` | 显示渲染音高 | 同上，线为虚线，与卷帘中的画法一致 |
| `showEnvelopes.svg` | 显示包络 | 音符条上方包络的轮廓 |
| `showParameters.svg` | 显示参数 | 音符条下方两行文字 |
| `crossfadeP2P3.svg` | 包络淡化（p2、p3） | 上方为数字 23，下方为包络的轮廓，上面两个点（p2、p3）各一个空心手柄 |
| `crossfadeP1P4.svg` | 包络淡化（p1、p4） | 上方为数字 14，下方为包络的轮廓，下面两个点（p1、p4）各一个空心手柄 |
| `resetEnvelope.svg` | 重置包络 | 上方为重置箭头，下方为包络轮廓 |
| `multiplyBy2.svg`、`multiplyBy3.svg` | 频率表选区 ×2、×3 | 左侧向上的箭头，右侧为数字 2 或 3。数字的笔画是 `crossfadeP2P3.svg` 中数字放大 1.6 倍 |
| `divideBy2.svg`、`divideBy3.svg` | 频率表选区 ÷2、÷3 | 同上，箭头向下 |
| `interpolate.svg` | 频率表选区插值 | 两个空心点之间一条直线 |
| `keyToAverage.svg` | 基准频率设为选区平均值 | 起伏的曲线与穿过其中央的水平线（基准频率线） |
| `playSpan.svg` | 从左空白播放到右空白 | 一对中括号（左空白与右空白）之间一个播放三角形。三角形的形状与绿色（`#208A3C`）取自 `../intellij/run.svg`，缩小为约 0.63 倍，与「播放音频文件」的图标一致，许可同 `../intellij/` |

四个显示开关共用一根音符条，所显示的内容按其在卷帘中的位置绘制：音高、渲染音高与包络在音符之上，参数在音符之下。音高、包络与参数三项的位置与 UTAU 一致。
