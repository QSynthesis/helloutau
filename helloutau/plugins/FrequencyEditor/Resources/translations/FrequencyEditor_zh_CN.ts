<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>hello::daw::MoresamplerDiagnoser</name>
    <message>
        <source>Clear the render cache after the change, because the fragments already written remain placeholders.</source>
        <translation>修改后清除渲染缓存，因为已写出的分片仍是占位文件。</translation>
    </message>
    <message>
        <source>Realtime playback cannot read the fragments of moresampler, which writes LLSM data instead of wave files.</source>
        <translation>实时播放无法读取 moresampler 的分片，moresampler 写出的是 LLSM 数据而不是 wav 文件。</translation>
    </message>
    <message>
        <source>Moresampler writes LLSM data instead of wave files, which the wavtool cannot join.</source>
        <translation>moresampler 写出的是 LLSM 数据而不是 wav 文件，合成器无法拼接。</translation>
    </message>
    <message>
        <source>The option resampler-compatibility is on in %1, so that moresampler wrote these fragments before the change.</source>
        <translation>%1 中的 resampler-compatibility 已为 on，因此这些分片是修改之前写出的。</translation>
    </message>
    <message>
        <source>The file %1 does not exist, in which case moresampler writes wave files, so that moresampler wrote these fragments before the change.</source>
        <translation>文件 %1 不存在，此时 moresampler 写出的是 wav 文件，因此这些分片是修改之前写出的。</translation>
    </message>
    <message>
        <source>Moresampler with the option off writes a placeholder text at the path of each fragment and the data beside it with the extension .llsm.tmp.</source>
        <translation>该选项为 off 时，moresampler 在每个分片的路径上写出一段占位文字，并把数据写入旁边扩展名为 .llsm.tmp 的文件。</translation>
    </message>
    <message>
        <source>With the option resampler-compatibility off in %1, moresampler writes a placeholder text at the path of each fragment and the data beside it with the extension .llsm.tmp. Only moresampler as the wavtool joins this data.</source>
        <translation>%1 中的 resampler-compatibility 为 off 时，moresampler 在每个分片的路径上写出一段占位文字，并把数据写入旁边扩展名为 .llsm.tmp 的文件。只有以 moresampler 作为合成器时才能拼接这些数据。</translation>
    </message>
    <message>
        <source>Set resampler-compatibility to on in %1.</source>
        <translation>在 %1 中把 resampler-compatibility 设为 on。</translation>
    </message>
    <message>
        <source>Alternatively, set the wavtool to moresampler as well.</source>
        <translation>或者把合成器也设为 moresampler。</translation>
    </message>
</context>
</TS>
