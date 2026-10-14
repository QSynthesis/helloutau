#include <filesystem>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QtEndian>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/FrequencyFormats.h>

#include <FrequencyEditor/DioFormat.h>

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

    // Returns a silent WAVE file of 16-bit mono samples
    QByteArray waveOf(qint32 rate, qint32 samples) {
        const QByteArray data(samples * 2, '\0');
        QByteArray bytes("RIFF");
        bytes += bytesOf<qint32>(36 + qint32(data.size())) + "WAVEfmt " + bytesOf<qint32>(16) +
                 bytesOf<qint16>(1) + bytesOf<qint16>(1) + bytesOf<qint32>(rate) +
                 bytesOf<qint32>(rate * 2) + bytesOf<qint16>(2) + bytesOf<qint16>(16);
        return bytes + "data" + bytesOf<qint32>(qint32(data.size())) + data;
    }

    constexpr double framePeriod = 5.80498866213152;

}

class test_DioFormat : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    fs::path pathOf(const char *name) const {
        return fs::path(m_dir.path().toStdU16String()) / name;
    }

private Q_SLOTS:
    // The format of world4utau and its caches
    void the_format_is_declared() {
        const DioFormat dio;
        QCOMPARE(dio.id(), QStringLiteral("dio"));
        QCOMPARE(dio.resamplerPatterns(),
                 (QStringList{QStringLiteral("w4u*.exe"), QStringLiteral("world4utau*.exe")}));
        QCOMPARE(dio.mapFileNameOf("a.wav"), std::optional<fs::path>("a.dio"));
        const auto caches = dio.analysisCaches();
        QCOMPARE(caches.size(), 2);
        QCOMPARE(caches.at(0).name, QStringLiteral("STAR"));
        QCOMPARE(caches.at(0).fileNameOf(pathOf("a.wav")), fs::path("a.star"));
        QCOMPARE(caches.at(1).fileNameOf("a.wav"), fs::path("a.platinum"));
    }

    // b.dio: the time of each frame as written, in seconds
    void a_dio_reads() {
        const DioFormat dio;
        QByteArray bytes("wrld-dio");
        bytes += bytesOf<qint32>(1000) + bytesOf<qint32>(44100) + bytesOf<qint32>(2);
        bytes += bytesOf(0.0) + bytesOf(0.0) + bytesOf(0.005) + bytesOf(261.6);
        write(pathOf("b.dio"), bytes);
        QCOMPARE(dio.mapsExist(pathOf(""), {"b.wav", "c.wav"}), (QList<bool>{true, false}));
        kit::DiagnosticList diagnostics;
        const auto table = dio.read(pathOf("b.wav"), 44100, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->frames.size(), size_t(2));
        QCOMPARE(table->frames[1].time, 5.0);
        QCOMPARE(table->frames[1].frequency, 261.6);
        QVERIFY(!table->frames[1].amplitude);
        QVERIFY(!table->averageFrequency);

        write(pathOf("b.dio"), "wrld-frq");
        QVERIFY(!dio.read(pathOf("b.wav"), 44100, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }

    // The caches of world4utau beside the dio follow it, after the map.
    void the_caches_follow_the_renames() {
        QDir(m_dir.path()).mkdir(QStringLiteral("r"));
        const auto directory = pathOf("r");
        write(directory / fs::path(u"偁.dio"), "wrld-dio");
        write(directory / fs::path(u"偁.star"), "star");
        const QList<kit::FrequencyFormat::AudioRename> renames{
            {fs::path(u"偁.wav"), "b.wav"},
            {"c.wav",             "d.wav"},
        };
        kit::DiagnosticList diagnostics;
        const auto changes = DioFormat().changesRenaming(directory, renames, diagnostics);
        QVERIFY(changes);
        QCOMPARE(changes->size(), 2);
        QCOMPARE(changes->at(0).path, fs::path(u"偁.dio"));
        QCOMPARE(changes->at(0).to, fs::path("b.dio"));
        QCOMPARE(changes->at(1).path, fs::path(u"偁.star"));
        QCOMPARE(changes->at(1).to, fs::path("b.star"));
        QVERIFY(diagnostics.isEmpty());
    }

    // The dio is removed with the caches of world4utau beside it.
    void a_dio_is_removed_with_its_caches() {
        QDir(m_dir.path()).mkdir(QStringLiteral("w"));
        const auto directory = pathOf("w");
        for (const auto name : {"a.dio", "a.star", "a.platinum", "a.wav"}) {
            write(directory / name, "x");
        }
        kit::DiagnosticList diagnostics;
        QVERIFY(kit::FrequencyFormats::removeMaps(DioFormat(), directory, {"a.wav", "b.wav"},
                                                  diagnostics));
        QVERIFY(diagnostics.isEmpty());
        QVERIFY(!fs::exists(directory / "a.dio"));
        QVERIFY(!fs::exists(directory / "a.star"));
        QVERIFY(!fs::exists(directory / "a.platinum"));
        QVERIFY(fs::exists(directory / "a.wav"));
    }

    // A dio read and written again is unchanged. The signal length and the sample rate of a new
    // dio map are those of the audio file, and its caches are removed if requested. Frames of a
    // period other than that of world4utau are not written, and neither is a new dio without its
    // audio file.
    void a_dio_is_written() {
        QDir(m_dir.path()).mkdir(QStringLiteral("x"));
        const auto directory = pathOf("x");
        const DioFormat dio;
        QVERIFY(dio.canWrite());
        QByteArray bytes("wrld-dio");
        bytes += bytesOf<qint32>(1000) + bytesOf<qint32>(44100) + bytesOf<qint32>(3);
        // The time of the last frame changes in the last bit if converted to milliseconds and
        // back.
        for (int i = 0; i < 3; ++i) {
            bytes += bytesOf(framePeriod * (i + 103) * 0.001) + bytesOf(i * 100.5);
        }
        write(directory / "a.dio", bytes);
        kit::DiagnosticList diagnostics;
        const auto map = dio.read(directory / "a.wav", 44100, diagnostics);
        QVERIFY(map);
        QCOMPARE(map->hop, 256.0);
        const auto same = dio.changesWriting(directory,
                                             {
                                                 {"a.wav", *map}
        },
                                             diagnostics);
        QVERIFY(same);
        QCOMPARE(same->size(), 1);
        QCOMPARE(same->at(0).path, fs::path("a.dio"));
        QCOMPARE(same->at(0).contents, bytes);

        auto edited = *map;
        edited.frames[1].frequency = 0;
        edited.hop = 48000 * framePeriod / 1000;
        write(directory / "b.wav", waveOf(48000, 4800));
        write(directory / "b.star", "star");
        QVERIFY(kit::FrequencyFormats::writeMaps(dio, directory,
                                                 {
                                                     {"b.wav", edited}
        },
                                                 true, diagnostics));
        QByteArray created("wrld-dio");
        created += bytesOf<qint32>(4800) + bytesOf<qint32>(48000) + bytesOf<qint32>(3);
        for (const auto &frame : edited.frames) {
            created += bytesOf(frame.time / 1000) + bytesOf(frame.frequency);
        }
        QCOMPARE(contentsOf(directory / "b.dio"), created);
        QVERIFY(!fs::exists(directory / "b.star"));
        QVERIFY(diagnostics.isEmpty());

        edited.hop = 256;
        QVERIFY(!dio.changesWriting(directory,
                                    {
                                        {"b.wav", edited}
        },
                                    diagnostics));
        QCOMPARE(diagnostics.size(), 1);
        QVERIFY(!dio.changesWriting(directory,
                                    {
                                        {"c.wav", *map}
        },
                                    diagnostics));
        QVERIFY(diagnostics.size() > 1);
    }

    // A dio that read() rejects is damaged and not repairable. A complete dio and an audio file
    // without a dio are not reported.
    void a_damaged_dio_is_reported() {
        QDir(m_dir.path()).mkdir(QStringLiteral("d"));
        const auto directory = pathOf("d");
        QByteArray bytes("wrld-dio");
        bytes += bytesOf<qint32>(1000) + bytesOf<qint32>(44100) + bytesOf<qint32>(1);
        bytes += bytesOf(0.0) + bytesOf(261.6);
        write(directory / "a.dio", bytes);
        write(directory / "b.dio", bytes.chopped(1));
        write(directory / "c.dio", "wrld-di");
        // Complete apart from the header
        write(directory / "f.dio", QByteArray("wrld-frq") + bytes.mid(8));
        const auto damages =
            DioFormat().checkMapFiles(directory, {"a.wav", "b.wav", "c.wav", "e.wav", "f.wav"});
        QCOMPARE(damages.size(), 3);
        QCOMPARE(damages.at(0).path, fs::path("b.dio"));
        QCOMPARE(damages.at(1).path, fs::path("c.dio"));
        QCOMPARE(damages.at(2).path, fs::path("f.dio"));
        QVERIFY(!damages.at(0).repairable);
        QVERIFY(!damages.at(0).description.isEmpty());
    }
};

QTEST_GUILESS_MAIN(test_DioFormat)

#include "test_DioFormat.moc"
