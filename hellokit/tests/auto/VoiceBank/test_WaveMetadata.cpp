#include <filesystem>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/WaveMetadata.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    QByteArray u32(quint32 value) {
        QByteArray bytes;
        for (int i = 0; i < 4; ++i) {
            bytes.push_back(char((value >> (8 * i)) & 0xff));
        }
        return bytes;
    }

    QByteArray chunk(const char *id, const QByteArray &content, bool pad = true) {
        auto bytes = QByteArray(id, 4) + u32(quint32(content.size())) + content;
        if (pad && content.size() % 2) {
            bytes.push_back('\0');
        }
        return bytes;
    }

    QByteArray riff(const QByteArray &chunks) {
        return "RIFF" + u32(quint32(4 + chunks.size())) + "WAVE" + chunks;
    }

    // 16-bit mono PCM at 1000 Hz
    const QByteArray format = QByteArray::fromHex("01000100e8030000d007000002001000");
    const QByteArray audio = QByteArray::fromHex("0100020003000400");

}

class test_WaveMetadata : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_plain_file_has_no_metadata() {
        DiagnosticList diagnostics;
        const auto plain = riff(chunk("fmt ", format) + chunk("data", audio));
        const auto report = WaveMetadata::find(plain, diagnostics);
        QVERIFY(report);
        QVERIFY(report->isEmpty());
        QCOMPARE(*WaveMetadata::stripped(plain, diagnostics), plain);
        QVERIFY(diagnostics.isEmpty());
    }

    // The chunks around the format and the audio go, and the sizes are written anew.
    void the_other_chunks_are_removed() {
        DiagnosticList diagnostics;
        const auto plain = riff(chunk("fmt ", format) + chunk("data", audio));
        const auto tagged =
            riff(chunk("JUNK", QByteArray(28, '\0')) + chunk("fmt ", format) +
                 chunk("LIST", "INFOISFTLavf0") + chunk("data", audio) + chunk("id3 ", "ID3abc"));
        const auto report = WaveMetadata::find(tagged, diagnostics);
        QVERIFY(report);
        QCOMPARE(report->chunks, (QList<QByteArray>{"JUNK", "LIST", "id3 "}));
        QCOMPARE(report->trailingBytes, 0);
        QCOMPARE(*WaveMetadata::stripped(tagged, diagnostics), plain);
    }

    // The format first, whatever the order, and a second data chunk counts as metadata.
    void the_format_goes_before_the_audio() {
        DiagnosticList diagnostics;
        const auto reversed =
            riff(chunk("data", audio) + chunk("fmt ", format) + chunk("data", "\x07\x07"));
        QCOMPARE(WaveMetadata::find(reversed, diagnostics)->chunks, QList<QByteArray>{"data"});
        QCOMPARE(*WaveMetadata::stripped(reversed, diagnostics),
                 riff(chunk("fmt ", format) + chunk("data", audio)));
    }

    // Bytes too few for a chunk, an odd size padded, and a size beyond the end of the file
    void the_sizes_follow_the_file() {
        DiagnosticList diagnostics;
        const auto trailing = riff(chunk("fmt ", format) + chunk("data", audio)) + "abc";
        QCOMPARE(WaveMetadata::find(trailing, diagnostics)->trailingBytes, 3);
        QCOMPARE(*WaveMetadata::stripped(trailing, diagnostics),
                 riff(chunk("fmt ", format) + chunk("data", audio)));

        // 8-bit audio of 3 samples, padded, whose pad the source lacks
        const auto odd = riff(chunk("fmt ", format) + chunk("data", "\x01\x02\x03", false));
        const auto padded = *WaveMetadata::stripped(odd, diagnostics);
        QCOMPARE(padded, riff(chunk("fmt ", format) + chunk("data", "\x01\x02\x03")));
        QCOMPARE(padded.size() % 2, 0);

        // A data chunk of 100 bytes of which the file holds 8
        auto cut = riff(chunk("fmt ", format)) + "data" + u32(100) + audio;
        QVERIFY(WaveMetadata::find(cut, diagnostics)->isEmpty());
        QCOMPARE(*WaveMetadata::stripped(cut, diagnostics),
                 riff(chunk("fmt ", format) + chunk("data", audio)));
        QVERIFY(diagnostics.isEmpty());
    }

    void a_file_without_format_or_audio_is_refused() {
        DiagnosticList diagnostics;
        QVERIFY(!WaveMetadata::find(QByteArray("RIFX\0\0\0\0WAVE", 12), diagnostics));
        QVERIFY(!WaveMetadata::find(riff(chunk("fmt ", format)), diagnostics));
        QVERIFY(!WaveMetadata::stripped(riff(chunk("data", audio)), diagnostics));
        QCOMPARE(diagnostics.size(), 3);
        QVERIFY(diagnostics.at(1).message.contains(QStringLiteral("data")));
        QVERIFY(diagnostics.at(2).message.contains(QStringLiteral("fmt")));
    }

    void a_file_is_rewritten_in_place() {
        QTemporaryDir dir;
        const auto path = fs::path(dir.path().toStdU16String()) / "a.wav";
        {
            QFile file(QString::fromStdU16String(path.u16string()));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(riff(chunk("fmt ", format) + chunk("data", audio) + chunk("LIST", "x")));
        }
        DiagnosticList diagnostics;
        QCOMPARE(WaveMetadata::find(path, diagnostics)->chunks, QList<QByteArray>{"LIST"});
        QVERIFY(WaveMetadata::strip(path, diagnostics));
        QVERIFY(WaveMetadata::find(path, diagnostics)->isEmpty());
        QVERIFY(diagnostics.isEmpty());

        QVERIFY(!WaveMetadata::strip(path.parent_path() / "missing.wav", diagnostics));
        QVERIFY(!WaveMetadata::find(path.parent_path() / "missing.wav", diagnostics));
        QCOMPARE(diagnostics.size(), 2);
    }
};

QTEST_GUILESS_MAIN(test_WaveMetadata)

#include "test_WaveMetadata.moc"
