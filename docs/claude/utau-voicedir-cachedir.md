# UTAU 对 VoiceDir 与 CacheDir 的处理

本文档记录实测得到的 UTAU 行为：`VoiceDir` 怎样解析为音源目录、保存时怎样写回，`CacheDir` 怎样确定，以及传给插件的临时文件中这两项的写法。结论来自 2026-09-28 在 Windows 上对 `utau.exe`（文件版本 0.04.0018）的实测，探针与操作步骤位于 `.cache/utau-probe/voicedir/`，可重复执行。

所有结论均为实测结果，而非推断。修改本文档前必须重新运行探针。

## VoiceDir 的解析

| 写法 | 解析为 |
|---|---|
| `%VOICE%<名称>` | `<utau.exe 所在目录>\voice\<名称>` |
| 相对路径，如 `hp_rel` | `<utau.exe 所在目录>\hp_rel` |
| 绝对路径 | 原样使用 |

**相对路径以 `utau.exe` 所在目录为基准**，既不是 UST 所在目录，也不是 `voice` 目录。探针在这三处各放一个同名的音源，UTAU 使用的是 `utau.exe` 旁边的那一个。工程是经由 UTAU 的「打开」对话框打开的，因此这一基准也不随打开文件时的当前目录变化。

## 保存时的写回

保存（上書き保存）与另存为（名前を付けて保存）的结果相同：

- **指向 `voice` 目录内的绝对路径改写为 `%VOICE%` 形式。** `<utau.exe 所在目录>\voice\hp_abs` 写回为 `%VOICE%hp_abs`。
- **相对路径与 `%VOICE%` 形式原样写回。**
- **`CacheDir` 一律写为 `<UST 文件名去掉扩展名>.cache`**，不论文件中原来写的是什么。普通保存同样改写：`CacheDir=orig-t1-relative.cache` 的文件保存后为 `CacheDir=t1-relative.cache`，另存为 `t1-saveas.ust` 后为 `CacheDir=t1-saveas.cache`。

## CacheDir 的确定

**UTAU 不使用文件中的 `CacheDir`。** 打开后，缓存目录即为 UST 所在目录下的 `<UST 文件名去掉扩展名>.cache`：探针文件写的是 `orig-<名称>.cache`，传给插件的临时文件中却是 `<UST 所在目录>\<名称>.cache`，保存前即是如此。

## 传给插件的临时文件

以下是全选音符后运行插件时，UTAU 写出的临时文件的 `[#SETTING]`，与工程文件的写法不同：

```
[#VERSION]
UST Version 1.20
[#SETTING]
Project=<UST 的绝对路径>
Tempo=120.00
VoiceDir=<解析后的绝对路径>
CacheDir=<UST 所在目录>\<名称>.cache
Mode2=True
```

- **`VoiceDir` 与 `CacheDir` 是解析后的绝对路径**，`%VOICE%` 与相对路径都已展开。插件因此无须知道 UTAU 的安装位置。
- 另有 `Project=`，是工程文件的绝对路径。
- 版本行写作 `UST Version 1.20`，工程文件中为 `UST Version1.2`。
- 没有 `Tracks`、`ProjectName`、`OutFile`、`Tool1`、`Tool2`。
- 每个音符另有 UTAU 计算出的只读条目：`@preuttr`、`@overlap`、`@stpoint`、`@filename`（样本文件名）、`@alias`（匹配到的别名）。

## 对 HelloUtau 的影响

- HelloUtau 不在 UTAU 的安装目录中运行，因此须由用户在设置中指定 UTAU 的安装目录，用于解析 `%VOICE%` 与相对路径。未指定时，这两种写法无法解析，须由用户为工程选择音源。
- 保存时 `CacheDir` 按上述规则改写。
- 将来调用原版 UTAU 插件时，临时文件须按上述写法生成，`VoiceDir` 与 `CacheDir` 写成解析后的绝对路径。
