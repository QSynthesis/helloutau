# 主题系统

本文档记录编辑器主题系统的设计来源、设计方向与实现。主题系统是 `helloutau` 模块的子库 `HelloUtauTheme`，见「已确定的事项」；已实现的部分见各「实现的第…块」。

设计沿用同一作者此前在 [qtmediate](https://github.com/stdware/qtmediate) 中的做法，其原理说明见 [qsynthesis-docs「3. 元类型」](https://github.com/SineStriker/qsynthesis-docs/tree/main/3.%20%E5%85%83%E7%B1%BB%E5%9E%8B)。两者的副本位于 `.cache/qtmediate` 与 `.cache/qsynthesis-docs`。

## 参考来源的使用限制

**qtmediate 只作为设计参考，不作为代码来源。** 本仓库沿用其机制与思想，代码按本仓库的规范重新编写，不复制其源文件、类型或函数实现。理由如下：

- qtmediate 的代码不符合本仓库的命名、目录与注释规范（`QM` / `C` 前缀、`Q_GLOBAL_STATIC` 登记表、静态初始化对象中的注册）。
- 其中存在若干缺陷，见「参考实现的缺陷」。
- 其样式表预处理基于正则替换，而非语法分析，见「参考实现的局限」。

## 样式表向自定义类型赋值

Qt 样式表中的 `qproperty-<name>: <value>` 在 polish 时写入控件的同名属性。`QStyleSheetStyle::setProperties` 只为 `QIcon`、`QImage`、`QPixmap`、`QRect`、`QSize`、`QColor`、`QBrush`、`QKeySequence` 按属性类型解析值；其他类型取第一个值元素的原始 `QVariant`，交给 `setProperty`，由 `QVariant` 的类型转换写入属性。已在 Qt 6.11.1 的 `qtbase/src/widgets/styles/qstylesheetstyle.cpp` 中核对，该行为与 Qt 5 相同。

值元素的原始形式由 `QCss::Parser` 决定：

- `func(...)` 解析为类型 `Function`，其 `QVariant` 为 `QStringList{"func", "<括号内原文>"}`（`qtbase/src/gui/text/qcssparser.cpp`）。
- 不含空白的普通词解析为 `QString`。
- 以空白分隔的多个元素中，只有第一个元素参与上述默认分支。

因此，只要为自定义类型登记从 `QStringList` 或 `QString` 出发的 `QMetaType` 转换函数，样式表即可直接为该类型的属性赋值。这一机制不修改 Qt，也不要求继承 `QStyle`，并允许一条声明同时设置一组相关字段（例如画笔的颜色、宽度与线型），控件因此不必为每个字段声明一个属性。

**该行为未写入 Qt 文档**，Qt 升级后须重新核对 `setProperties` 的默认分支。

## 值语法

qtmediate 在上述机制之上规定了函数式的值语法。本仓库沿用其思想，具体语法在实现时另行确定。

- **函数名选择类型**：`qpen(...)`、`qfont(...)`、`qrect(...)`、`qmargins(...)`，每种类型登记一个函数名。容器类型 `qlist(...)` 与 `qmap(k=v, ...)` 的元素按形式推断类型：`#` 开头为颜色，`rgb(...)` 等为颜色，`1px` 为像素尺寸，`1px 2px` 为 `QSize`，已登记的函数名为对应类型。
- **参数**：先写位置参数，后写关键字参数（`name=value`），写出关键字参数后不再接受位置参数。未写的参数取默认值，或取另一个参数的值。
- **按钮状态**：可交互元素区分未选中与选中两组，每组有弹起、悬停、按下、禁用四个状态，共八个状态。支持按钮状态的字段写作一个括号组，例如 `(white, down=grey)`。未指定的状态按固定规则回落：悬停回落到弹起，按下回落到悬停，禁用回落到弹起，选中组的弹起回落到未选中组的弹起，选中组内部的回落规则与未选中组相同。

示例：

```css
qpen((lightgrey, down=white), 1px, solid, flat, bevel)
qfont(#FFFFFF, 15pt, family="Times New Roman")
qrect(white, (1px, 2px), 3px)
```

## 可着色的 SVG 图标

**动机**：同一图标在不同主题和不同按钮状态下使用不同颜色，而不为每种颜色复制一份 SVG 文件。SVG 规定的 `currentColor` 关键字表示「当前颜色」，Qt 的 SVG 渲染不支持由外部指定该值。

qtmediate 的实现方式（svgx）分为三部分：

1. **图标引擎的选择**：`QIcon(fileName)` 按文件名后缀选择图标引擎插件（`qtbase/src/gui/image/qicon.cpp` 的 `iconEngineFromSuffix`）。qtmediate 静态注册一个键为 `svgx` 的 `QIconEnginePlugin`，并将图标参数序列化为以 `.svgx` 结尾的「文件名」，例如 `[[("a.svg", up2="b.svg"), (#FFF, down=grey)]].svgx`。样式表中的 `svg(...)` 在预处理时转写为 `url("[[...]].svgx")`，经 Qt 的图标解析进入该引擎。
2. **状态与颜色**：`QIcon` 不含按钮状态。控件在绘制前取得图标引擎，通过 `QIconEngine::virtual_hook` 的自定义编号设置当前按钮状态，引擎按状态选择 SVG 文件与颜色，在 SVG 文本中将 `currentColor` 替换为该颜色后渲染。渲染结果放入 `QPixmapCache`，缓存键包含尺寸、状态、颜色与序号，切换主题时清空缓存。
3. **颜色跟随文字**：颜色写作 `auto` 时，控件以一个占位字符代替图标和文字，将按钮绘制到一张图片上，并替换该图片的绘制引擎以截获绘制文字时使用的画笔，取其颜色作为图标颜色。图标颜色因此与样式表中的文字颜色一致，不必另行指定。

取得图标引擎与替换绘制引擎都依赖 Qt 私有头文件（`qicon_p.h`、`qpaintengine_raster_p.h`）。

## 扩展语法与预处理

qtmediate 在样式表交给 Qt 之前做一次文本转换：

| 扩展写法 | 转换结果 | 用途 |
|---|---|---|
| `--key: value` | `qproperty-key: value` | 与 CSS 自定义属性的写法一致 |
| `---key: value` | `key: value` | 书写值不符合 CSS 语法的标准属性 |
| `:not(:checked)` | `:!checked` | CSS 的否定伪类写法 |
| `url(@/a.png)` | 样式表文件所在目录下的绝对路径 | 相对路径 |
| `svg(...)` | `url("[[...]].svgx")` | 可着色的 SVG 图标 |
| `Npx` | 按缩放比例换算后的 `Npx` | 界面缩放；`font-size` 另有独立比例 |

在 Qt 6 中，高 DPI 缩放由 Qt 完成，像素换算只用于用户设置的界面缩放与字号缩放。qtmediate 仅在 Qt 5 下按屏幕 DPI 换算。

## 主题的组织

- **主题描述文件**：在主题目录中递归查找 `*.res.json`。每个文件声明变量、控件标识到命名空间的映射，以及每个主题、每个命名空间的样式表（文件路径或内联文本，可带优先级与缩放比例；数值与字符串可按平台分别给出）。
- **变量与继承**：变量按主题分组，样式表中的变量引用在应用前替换。主题以变量 `_base` 指定所继承的主题，应用时按继承链依次拼接各主题的样式表，并检测循环引用。名为 `_common` 的主题先于当前主题应用。
- **控件登记**：控件以一个或多个标识登记（`installTheme(widget, id)`）。主题、缩放比例或所在屏幕变化时，重新拼接该控件对应的样式表并调用 `setStyleSheet`。连续的参数修改合并为一次延迟刷新。

## 参考实现的局限

- **依赖未公开行为与私有头文件**：`setProperties` 的默认分支、`QCss` 解析器、`QIcon` 与光栅绘制引擎的私有数据。Qt 升级时须重新核对。
- **预处理基于正则替换**：`svg\((.*?)\)` 取最短匹配，参数中出现括号时结果错误；像素换算会改写引号内的文字，例如文件名中的 `12px`。
- **属性只在 polish 时写入**：属性值在运行时改变后，须重新 polish 才能生效。
- **每个接收类型各自解析按钮状态**：各类型的 `fromStringList` 重复实现相同的参数与状态处理。

## 参考实现的缺陷

以下缺陷见于 qtmediate `b0aacdd`，本仓库的实现不得重现：

- `QMCss::parseBoolean` 对 `"false"` 返回 `true`。
- `QMPrivate::serializeSvgxArgs` 的第二个循环遍历 `fileMap` 并写入 `fileArgs`，颜色参数始终为空；输出的状态键名 `hover` / `hover2` 与解析时接受的 `over` / `over2` 不一致。
- `HackPaintEngine` 的构造函数未将参数 `items` 赋给成员，成员未初始化。
- `readStyleSheets` 以 `--map.begin()` 作为反向遍历的终点，行为未定义。

## 设计方向

延续的思想：

- 样式表是主题的唯一描述手段，自定义外观通过属性与登记的类型转换写入控件，不继承 `QStyle`。
- 一条声明设置一组相关字段；按钮状态作为通用的一层，由所有支持状态的字段共用，不由各类型分别实现。
- 图标以 SVG 编写，颜色由主题与按钮状态决定，可跟随文字颜色。
- 主题由描述文件、变量、继承链与命名空间组成，控件按标识登记。

## 已确定的事项

**接受 qtmediate 所用范围内的 Qt 私有头文件**，即 QtCore、QtGui、QtWidgets 三个模块的私有部分，包括 `qicon_p.h`、`qpaintengine_raster_p.h`、`qcssparser_p.h` 等。超出这一范围时须先与作者确定。同样的效果能以公开接口实现时优先使用公开接口，以减少 Qt 升级时须核对的内容。已知的一处：

- **按钮状态不能只靠 `QIcon::Mode` 与 `QIcon::State` 传给图标引擎。** Qt 自带的样式在按钮获得焦点时给出 `Active`，工具按钮只在自动浮起且悬停时给出 `Active`，按下的状态从不传递（Qt 6.11.1 `qtbase/src/widgets/styles/qcommonstyle.cpp`）。**需要这些状态的控件重写绘制，经公开接口取得固定于该状态的图标**（`ThemeIcon::forState()`，作者 2026-09-28 确定），不以 `qicon_p.h` 取得并改写共享的图标引擎，理由见「实现的第四块」。

**值语法与扩展语法由一个分词器解析，不使用正则替换。** 分词器识别字符串、注释与括号的嵌套，供两处共用：

- 预处理样式表时，据此找到扩展写法，`svg(...)` 的参数中含括号、引号内含 `12px` 均不致出错。
- 解析值时，得到统一的结构：函数名、位置参数、关键字参数、按钮状态组。各类型登记的转换函数接收这一结构，不再各自从 `QStringList` 解析；按钮状态的回落规则在通用层实现一次。

无法解析的值以 `qCWarning` 报告，写明所在位置，不静默采用默认值。

## 实现的第一块：语法（2026-09-28 起，作者确认前可改）

**值语法沿用 qsynthesis-docs「3. 元类型」**（函数名、位置参数与关键字参数、按钮状态组、`qlist` 与 `qmap` 的元素推断），只在以下几处明确或改变：

- **按钮状态的键名**为 `up`、`over`、`down`、`disabled` 与选中组的 `up2`、`over2`、`down2`、`disabled2`，位置参数按这一顺序。qtmediate 输出 `hover` 而解析 `over` 的不一致不再存在：只有 `over`。
- **含 `=` 或逗号的文字**须写成字符串（单引号或双引号，反斜杠转义），与原文档相同；关键字只能是词。
- **写出关键字参数后再写位置参数是错误**，报告其位置，不静默忽略。
- **词**是除空白、逗号、括号、等号、引号以外的连续字符，`#FFF`、`1px`、`solid`、`-2px` 都是词；词后紧跟 `(` 即为函数。以空白分隔的多个值组成一个序列（`2px 2px`）。

**组成**（`helloutau/lib/Theme/`）：

| 类 | 职责 |
|---|---|
| `ThemeValue`、`ThemeSyntax` | 分词与解析：得到词、字符串、函数、括号组或序列，每个值记着它在原文中的位置，供报告错误 |
| `ThemeStates<T>` | 八种按钮状态的值与回落规则；从单个值或状态组读出 |
| `ThemeReader` | 基本值的读取：颜色（`#RGB`、`#RRGGBB`、`#AARRGGBB`、颜色名、`rgb()`、`rgba()`、`hsv()`、`hsva()`、`hsl()`、`hsla()`）、像素尺寸（`1px`）、整数、实数、布尔、字符串 |
| `ThemeStyleSheet` | 样式表的预处理，以同一分词规则跳过字符串与注释：`--key` 转为 `qproperty-key`，`---key` 去掉前缀，`:not(:x)` 转为 `:!x`，`url(@/a)` 转为相对样式表所在目录的路径，`Npx` 按界面缩放比例换算（`font-size` 另有比例）|

## 实现的第二块：样式表赋值的类型

**「样式表向自定义类型赋值」一节的机制已在 Qt 6.11.1 上验证**（`test_ThemeTypes`）：Qt 把 `qpen(...)` 以 `QStringList{"qpen", "<括号内原文>"}`、把单个词以 `QString` 交给 `setProperty`，经登记的 `QMetaType` 转换写入属性。转换函数返回 `std::optional`，无法解析时转换失败、属性保持原值，并以 `qCWarning`（类别 `hello.theme`）报告位置；函数名不符（例如给画笔属性写 `qfont(...)`）同样报告而不采用。单个词作为第一个参数（`qproperty-pen: red`）。

| 类型 | 写法 | 参数 |
|---|---|---|
| `ThemePen` | `qpen(...)` | color（按钮状态）、width、style、cap、join、dashPattern、dashOffset、miterLimit、cosmetic，与 qsynthesis-docs 相同；虚线长度以像素写，换算为 Qt 以线宽为单位的长度 |
| `ThemeFont` | `qfont(...)` | color（按钮状态）、size（`px` 或 `pt`）、weight（数值或 `thin` 到 `black`）、italic、family（一个名称或一组）。未写的字段保留所应用字体的值 |
| `ThemeRect` | `qrect(...)` | color（按钮状态）、margins（一个、两个即上下与左右、四个即左上右下）、radius |
| `ThemeShadow` | `qshadow(...)` | color、blur、offset（一个或两个长度）。qsynthesis-docs 中没有，为命令面板等弹出层的阴影新增 |

`QMargins` 与 `QStringList` 都是 Qt 的内置类型，二者之间不能登记转换，因此边距只作为 `ThemeRect` 的字段，不单独成为 `qmargins(...)`。

**使用这些类型的控件在构造时调用 `ThemeTypes::registerConversions()`**（可重复调用），以保证样式表生效前转换已登记。第一个使用者是命令面板：`qproperty-shadow: qshadow(#40000000, 16px, 0 4px)` 为它加上阴影，未设置时没有阴影。样式表中带命名空间的类名写作 `hello--daw--CommandPalette`。

## 实现的第三块：主题的组织

`ThemeManager` 实现「主题的组织」一节，描述文件的格式沿用 qtmediate：

```json
{
    "config": { "priority": 1, "ratio": 1 },
    "widgets": { "MainWindow": ["window", "roll"] },
    "variables": {
        "light": { "text": "#000000" },
        "dark": { "_base": "light", "text": { "value": "#EEEEEE", "priority": 2 } }
    },
    "stylesheets": {
        "_common": { "window": { "content": "A { margin: ${gap}; }" } },
        "light": { "window": { "file": "light/window.qss" }, "roll": [ { "content": "..." } ] }
    }
}
```

- 描述文件为搜索路径下任意层的 `*.res.json`，按路径排序后依次读取。`file` 相对描述文件所在目录，样式表中的 `url(@/...)` 相对样式表文件所在目录。
- **按平台给值**：键全为 `win`、`windows`、`mac`、`macos`、`linux` 的对象按当前系统取值，没有当前系统的键即为未给出；其他对象（如 `{ "value": ..., "priority": ... }`）照常解读。
- **组装**：控件的各标识映射到的命名空间依次收集；先 `_common` 的继承链，再当前主题的继承链（`_base`，由最基础的主题开始，检测循环）；同一主题内按优先级升序稳定排序，同一优先级按命名空间的顺序、再按文件与数组中的顺序。
- **变量**：`${name}` 以整条链（含 `_common`）合并后的变量替换，派生主题覆盖其基础主题，因此基础主题的样式表也使用派生主题的值（与 qtmediate 不同：它以每个主题自己的变量替换其样式表）。同一主题中同名变量取优先级高者，相同时取后读到的（与 qtmediate 相反，它取优先级数值小者；这里与样式表统一为数值大者胜出）。未定义的变量原样保留并报告。
- **比例**：每个样式表的 `ratio` 乘以管理器的界面缩放比例与字号比例，交给 `ThemeStyleSheet` 换算。
- **刷新**：主题、比例改变或重新读取文件后，已登记的控件在事件循环中统一重新设置一次样式表；控件销毁后自动移除。

**编辑器的内置主题**位于 `helloutau/lib/Editor/themes/`，编入资源 `:/helloutau/themes`。`Editor` 持有一个 `ThemeManager`，以该资源为搜索路径；每个主窗口以标识 `MainWindow` 登记，其样式表对子控件同样生效。内置主题目前只有 `_common` 的命令面板阴影；浅色与深色主题的视觉设计须经作者确认后再加入。

**主题系统是独立的子库 `HelloUtauTheme`**，位于 `helloutau/lib/Theme/`，头文件以 `<helloutau/Theme/...>` 引用，`HelloUtauWidgets` 与 `HelloUtauEditor` 依赖它。只有该子库链接 Qt 的私有模块，私有依赖因此集中在一处，也可以单独测试。命名空间为 `hello::daw`，不增加第三层：主题系统不会移出本仓库，不属于 `docs/Development.md` 所述的例外。类名以 `Theme`、`Svgx` 等为前缀，避免与其他子库的类重名。

## 实现的第四块：可着色的 SVG 图标（作者确认前可改）

`ThemeIcon` 实现「可着色的 SVG 图标」一节的前两部分。写法为 `svg(file, color)`，两个参数都可带按钮状态：

```css
QToolButton { qproperty-icon: svg("@/play.svg", (#333333, over=#000000, disabled=#999999)); }
QToolButton { qproperty-icon: svg(("@/play.svg", up2="@/pause.svg"), auto); }
```

- **样式表**：`ThemeStyleSheet` 把 `svg(...)` 转为 `url("<描述>.svgx")`，文件写作 `@/...` 时相对样式表所在目录。无法解析的 `svg(...)` 以 `qCWarning` 报告并原样保留，Qt 忽略该声明。
- **描述文件名**：参数以值语法写出（只写与回落结果不同的状态），整体以百分号编码，加后缀 `.svgx`，因此不含目录分隔符与引号。QIcon 按后缀选择图标引擎（`qtbase/src/gui/image/qicon.cpp` 的 `iconEngineFromSuffix`）；引擎以静态插件的形式编入 `HelloUtauTheme`（`ThemeIconPlugin`，键 `svgx`），库加载时即登记。
- **颜色**：文件中的 `currentColor` 替换为 `#RRGGBB`。QtSvg 读以 `#` 开头的颜色时不读透明度（`qtsvg/src/svg/qsvghandler.cpp` 的 `resolveColor`），因此颜色的透明度作为整个图标的不透明度。写作 `auto` 或不写颜色即跟随文字：控件给出文字颜色时用它，否则用调色板的 `WindowText`（禁用状态取 `Disabled` 组）。
- **状态**：QIcon 传来的模式与状态解读为：`Active` 与 `Selected` 为 over，`Disabled` 为 disabled，`On` 为选中组。按下等 QIcon 无法表达的状态，由控件以 `ThemeIcon::forState()` 取得固定于该状态（及文字颜色）的新 QIcon 再绘制。
- **缓存**：文件内容按路径缓存；图像放入 `QPixmapCache`，键含代次、尺寸、颜色与路径。`ThemeManager::reload()` 调用 `ThemeIcon::clearCache()` 递增代次，不清空整个 `QPixmapCache`。
- **适用范围（作者确定，2026-09-28）**：只针对两种图标，一是样式表以 `svg(...)` 给出的，二是代码中有意设置的 `ThemeIcon`（`ThemeIcon::icon()`）。主题更新时，登记的控件统一重设一次样式表即可，控件无须为此编写代码（`test_ThemeManager` 的 `installed_icons_follow_the_theme`）。需要表达按下等 QIcon 无法传递的状态的控件，自行重写绘制，以 `forState()` 取得该状态的图标；不另设事件过滤器替控件切换状态。

**按钮状态经公开接口传递（作者 2026-09-28 确定）。** 最初的设计沿用 qtmediate，以 `qicon_p.h` 取得引擎，再经 `virtual_hook` 设置状态。实现改用公开接口：引擎的 `iconName()` 返回描述文件名，`ThemeIcon::of()` 由 `QIcon::name()` 取回描述，`forState()` 据此新建一个引擎。理由是不改写共享的引擎：Qt 把一条样式表声明解析出的 QIcon 缓存在该声明中（`qtbase/src/gui/text/qcssparser.cpp` 的 `Declaration::iconValue`），使用同一规则的控件共用一个引擎；qtmediate 在绘制前改写该引擎的状态与颜色，未调用钩子的控件（菜单、普通工具按钮）因此沿用别的控件最后留下的状态与颜色。代价是 `QIcon::name()` 对这类图标返回描述文件名；Qt 6.11.1 只在 `QIcon::hasThemeIcon` 与 Linux 的 D-Bus 托盘图标中读取该名称，二者都不涉及这类图标。

**尚未实现**：

- 「颜色跟随文字」中替换绘制引擎以截获文字颜色的部分（`qpaintengine_raster_p.h`），留待第一个使用图标的控件。
- 控件须设置 `Qt::WA_Hover`，悬停时才会重绘，over 的颜色才会出现；样式表中没有依赖悬停状态的选择器时，Qt 不为控件设置该属性（`qtbase/src/widgets/styles/qstylesheetstyle.cpp` 的 `polish`）。
