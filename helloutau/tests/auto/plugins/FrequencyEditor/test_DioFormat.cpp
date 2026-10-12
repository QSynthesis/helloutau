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
};

QTEST_GUILESS_MAIN(test_DioFormat)

#include "test_DioFormat.moc"
