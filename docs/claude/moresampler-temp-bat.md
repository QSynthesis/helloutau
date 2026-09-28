# moresampler 0.8.4 对 `temp.bat` 的读取

2026-09-29 以 Ghidra（工程 `moresampler_exe_084`，程序 `moresampler.exe`）反编译得出。地址均为该 exe 的虚拟地址。本文是逆向所得，代码注释不得引用本文。

## 结论

moresampler 作为 wavtool（链式调用，即 UTAU 的调用方式）每次被调用时，都判断本次是否为最后一个音符；是则进入 mode 5，把此前追加的数据索引合成为最终的 wav。判断有两个来源，满足其一即可：

1. **命令行的最后一个参数为 `LAST_NOTE`**（readme 的 0.8.4 更新记录：「add LAST_NOTE as the last command line argument in wavtool mode to trigger mode 5」）。
2. **`<当前工作目录>\temp.bat` 中最后一个 `@set temp=` 行**指向本次调用的输入文件（`check_temp_last`，`FUN_00405510`）。

两者都不满足时，moresampler 只追加索引，不写出最终的 wav。

## `check_temp_last` 的规则

wavtool 主函数 `FUN_00419e20`，在 mode 4 追加索引之后：

- 路径为 `_wgetcwd()`（`FUN_004354d0`）加上 `L"\\temp.bat"`，即**进程的当前工作目录**下的 `temp.bat`，与 exe 的位置和 wavtool 的参数无关。打不开时记录 `check_temp_last: cannot open temp directory.`，视为不是最后一个音符。
- 从文件末尾向前读，每次多读 1000 字节，直至找到至少一个以 `@set temp=` 开头的行（`wcsncmp(line, L"@set temp=", 10)`，区分大小写，前面不得有空白，**`@set "temp=...` 的写法不匹配**）。取所读范围内最后一个这样的行。
- 取该行在最后一个 `/` 或 `\` 之后的部分，即文件名。
- 取本次调用第二个参数（输入文件，`FUN_00404ce0` 取文件名）的文件名。
- 两个文件名各自截到**第一个 `.` 或 `_` 之前**（以 `.` 或 `_` 开头的名字不截），比较相等即为最后一个音符。

UTAU 的缓存文件名以音符序号开头（如 `12_あ_C4_abcdef.wav`），因此比较的实际是音符序号：`temp.bat` 中最后一个 `@set temp=` 属于最后一个有声音符，该音符的 wavtool 调用触发 mode 5。休止符的 wavtool 调用输入为 `R.wav`，前缀为 `R`，不会触发。最后一个有声音符之后若还有休止符，它们的 wavtool 调用发生在 mode 5 之后（此时输出文件已是 wav），其效果尚未分析。

## 对 helloutau 的影响

- `ClassicSynthRunner` 的安全模式（`Quoting::Escaped`）写的是 `@set "temp=..."`，**不匹配**，因此以 moresampler 为 wavtool 时不会进入 mode 5，渲染不出最终的 wav。`docs/Synth.md` 中「转义改变了文本，可能导致这类引擎出错，尚未实测」由此确认。
- 修正方向：`temp` 一行写成 `@set temp=<值>`，以 `^` 转义 cmd 的特殊字符并把 `%` 加倍。moresampler 只取最后一个分隔符之后的文件名，且只比较第一个 `.` 或 `_` 之前的部分，目录中的 `^` 不影响判断。
- 在 Linux 上经 moreloader 运行时，脚本是 `temp.sh`，moresampler 找不到 `temp.bat`。要么在 moresampler 的工作目录另放一个不执行的 `temp.bat`（只需 `@set temp=` 行正确），要么在最后一个有声音符的 wavtool 调用末尾加 `LAST_NOTE`。
