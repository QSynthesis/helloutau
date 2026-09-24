# 主题系统

本文档记录编辑器主题系统的设计来源与设计方向。主题系统尚未实现，将属于 `helloutau` 模块（Qt Widgets）。

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

实现前须与作者确定的事项：

- 是否接受对 Qt 私有头文件的依赖，以及依赖的范围。
- 值语法与扩展语法的解析方式。候选做法是以分词器代替正则替换。
- 主题系统所在的子库与命名空间。
