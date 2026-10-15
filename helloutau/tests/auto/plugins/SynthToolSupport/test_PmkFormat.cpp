#include <filesystem>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QtEndian>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/FrequencyFormats.h>

#include <SynthToolSupport/PmkFormat.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    template <class T>
    QByteArray bytesOf(T value) {
        QByteArray bytes(sizeof(T), '\0');
        qToLittleEndian(value, bytes.data());
        return bytes;
    }

    void write(const fs::path &path, const QByteArray &bytes) {
        QFile file(QString::fromStdU16String(path.u16string()));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
    }

    QByteArray contentsOf(const fs::path &path) {
        QFile file(QString::fromStdU16String(path.u16string()));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    // Returns a pmk of the base period and the marks, each the end and the period
    QByteArray pmkOf(double basePeriod, const QList<std::pair<qint32, qint32>> &marks) {
        QByteArray bytes("\x13\x00", 2);
        bytes += bytesOf(basePeriod) + bytesOf(qint32(marks.size()));
        for (const auto &[end, period] : marks) {
            bytes += bytesOf(end) + bytesOf(period);
        }
        return bytes;
    }

    // The marks of a frame at 441 Hz followed by an unvoiced frame, as frqeditor writes them
    const QList<std::pair<qint32, qint32>> writtenMarks = {
        {100, 100},
        {200, 100},
        {300, 100},
        {349, 49 },
        {398, 49 },
        {447, 49 },
        {496, 49 },
        {545, 49 },
    };

}

class test_PmkFormat : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    fs::path pathOf(const char *name) const {
        return fs::path(m_dir.path().toStdU16String()) / name;
    }

private Q_SLOTS:
    // The format of TIPS, named after the audio file as frq is, with frames of 256 samples at
    // 44100 Hz
    void the_format_is_declared() {
        const PmkFormat pmk;
        QCOMPARE(pmk.id(), QStringLiteral("pmk"));
        QCOMPARE(pmk.resamplerPatterns(), QStringList{QStringLiteral("tips*.exe")});
        QCOMPARE(pmk.mapFileNameOf("a.wav"), std::optional<fs::path>("a_wav.pmk"));
        QVERIFY(pmk.analysisCaches().isEmpty());
        QCOMPARE(pmk.hopOf(44100), 256.0);
        QCOMPARE(pmk.hopOf(48000), 48000 * 256 / 44100.0);
    }

    // Each frame has the period of the mark that covers its start. The frames reach the end of
    // the last period. A period below 50 samples is unvoiced, and so is a base period below 50.
    void a_pmk_reads() {
        const PmkFormat pmk;
        write(pathOf("b_wav.pmk"), pmkOf(100, writtenMarks));
        QCOMPARE(pmk.mapsExist(pathOf(""), {"b.wav", "c.wav"}), (QList<bool>{true, false}));
        kit::DiagnosticList diagnostics;
        const auto table = pmk.read(pathOf("b.wav"), 48000, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->hop, 48000 * 256 / 44100.0);
        QCOMPARE(table->averageFrequency, std::optional(441.0));
        QCOMPARE(table->frames.size(), size_t(3));
        QCOMPARE(table->frames[0].frequency, 441.0);
        QCOMPARE(table->frames[1].frequency, 441.0);
        QCOMPARE(table->frames[2].frequency, 0.0);
        QCOMPARE(table->frames[2].time, 2 * 256 * 1000 / 44100.0);
        QVERIFY(!table->frames[0].amplitude);
        QVERIFY(diagnostics.isEmpty());

        write(pathOf("b_wav.pmk"), pmkOf(30, {}));
        const auto empty = pmk.read(pathOf("b.wav"), 44100, diagnostics);
        QVERIFY(empty);
        QVERIFY(empty->frames.empty());
        QCOMPARE(empty->averageFrequency, std::optional(0.0));

        write(pathOf("b_wav.pmk"), pmkOf(100, writtenMarks).replace(0, 1, "\x14"));
        QVERIFY(!pmk.read(pathOf("b.wav"), 44100, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }

    // The marks of each frame continue until the frame ends. A new pmk has the base period of
    // the average frequency, and an existing pmk keeps its base period if the average frequency
    // is unchanged. Frames of another hop are not written.
    void a_pmk_is_written() {
        QDir(m_dir.path()).mkdir(QStringLiteral("x"));
        const auto directory = pathOf("x");
        const PmkFormat pmk;
        QVERIFY(pmk.canWrite());
        kit::FrequencyMap map;
        map.hop = 256;
        map.averageFrequency = 441.0;
        map.frames = {
            {0,   441.0, {}},
            {5.8, 0.0,   {}},
        };
        kit::DiagnosticList diagnostics;
        QVERIFY(kit::FrequencyFormats::writeMaps(pmk, directory,
                                                 {
                                                     {"a.wav", map, 44100}
        },
                                                 false, diagnostics));
        QCOMPARE(contentsOf(directory / "a_wav.pmk"), pmkOf(100, writtenMarks));
        QVERIFY(diagnostics.isEmpty());

        // 44100 divided by the average frequency of 106.9 differs from 106.9 in the last bit.
        write(directory / "b_wav.pmk", pmkOf(106.9, {}));
        const auto read = pmk.read(directory / "b.wav", 44100, diagnostics);
        QVERIFY(read);
        map.averageFrequency = read->averageFrequency;
        const auto kept = pmk.changesWriting(directory,
                                             {
                                                 {"b.wav", map, 44100}
        },
                                             diagnostics);
        QVERIFY(kept);
        QCOMPARE(kept->at(0).contents.mid(2, 8), bytesOf(106.9));

        map.averageFrequency = 50.0;
        const auto low = pmk.changesWriting(directory,
                                            {
                                                {"c.wav", map, 44100}
        },
                                            diagnostics);
        QVERIFY(low);
        QCOMPARE(low->at(0).contents.mid(2, 8), bytesOf(0.0));

        QVERIFY(!pmk.changesWriting(directory,
                                    {
                                        {"c.wav", map, 48000}
        },
                                    diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }

    // A pmk that read() rejects is damaged and not repairable, among them a pmk with more than
    // markHop frames for each mark, here a single mark that ends near the largest int32.
    void a_damaged_pmk_is_reported() {
        QDir(m_dir.path()).mkdir(QStringLiteral("d"));
        const auto directory = pathOf("d");
        const auto bytes = pmkOf(100, writtenMarks);
        write(directory / "a_wav.pmk", bytes);
        write(directory / "b_wav.pmk", bytes.chopped(1));
        write(directory / "c_wav.pmk", pmkOf(100, {
                                                      {0x7fffff00, 100}
        }));
        write(directory / "f_wav.pmk", pmkOf(100,
                                             {
                                                 {-100, 100}
        })
                                           .replace(10, 4, bytesOf(-1)));
        const auto damages =
            PmkFormat().checkMapFiles(directory, {"a.wav", "b.wav", "c.wav", "e.wav", "f.wav"});
        QCOMPARE(damages.size(), 3);
        QCOMPARE(damages.at(0).path, fs::path("b_wav.pmk"));
        QCOMPARE(damages.at(1).path, fs::path("c_wav.pmk"));
        QCOMPARE(damages.at(2).path, fs::path("f_wav.pmk"));
        QVERIFY(!damages.at(0).repairable);
        QVERIFY(!damages.at(0).description.isEmpty());
    }
};

QTEST_GUILESS_MAIN(test_PmkFormat)

#include "test_PmkFormat.moc"
