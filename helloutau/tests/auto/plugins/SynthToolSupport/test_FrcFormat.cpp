#include <filesystem>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QtEndian>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/FrequencyFormats.h>

#include <SynthToolSupport/FrcFormat.h>

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

    // Returns a map in the layout of frq with the reserved bytes reserved
    QByteArray frcOf(qint32 hop, double average, const QByteArray &reserved,
                     const QList<std::pair<double, double>> &frames) {
        QByteArray bytes("FREQ0003");
        bytes += bytesOf(hop) + bytesOf(average) + reserved + bytesOf(qint32(frames.size()));
        for (const auto &[frequency, amplitude] : frames) {
            bytes += bytesOf(frequency) + bytesOf(amplitude);
        }
        return bytes;
    }

    const QByteArray model4Reserved = QByteArray("m4      ") + QByteArray(8, '\0');

}

class test_FrcFormat : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    fs::path pathOf(const char *name) const {
        return fs::path(m_dir.path().toStdU16String()) / name;
    }

private Q_SLOTS:
    // The format of model4, named after the audio file as frq is
    void the_format_is_declared() {
        const FrcFormat frc;
        QCOMPARE(frc.id(), QStringLiteral("frc"));
        QCOMPARE(frc.resamplerPatterns(), QStringList{QStringLiteral("model4*.exe")});
        QCOMPARE(frc.mapFileNameOf("a.wav"), std::optional<fs::path>("a_wav.frc"));
        QCOMPARE(frc.mapFileNameOf("a.b.wav"), std::optional<fs::path>("a.b_wav.frc"));
        QVERIFY(frc.analysisCaches().isEmpty());
        QCOMPARE(frc.hopOf(48000), 256.0);
    }

    // b_wav.frc: the times follow from the hop and the sample rate.
    void a_frc_reads() {
        const FrcFormat frc;
        write(pathOf("b_wav.frc"), frcOf(256, 220.0, model4Reserved,
                                         {
                                             {0.0,   10.0 },
                                             {220.5, 300.0}
        }));
        QCOMPARE(frc.mapsExist(pathOf(""), {"b.wav", "c.wav"}), (QList<bool>{true, false}));
        kit::DiagnosticList diagnostics;
        const auto table = frc.read(pathOf("b.wav"), 44100, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->hop, 256.0);
        QCOMPARE(table->averageFrequency, std::optional(220.0));
        QCOMPARE(table->frames.size(), size_t(2));
        QCOMPARE(table->frames[1].time, 256 * 1000 / 44100.0);
        QCOMPARE(table->frames[1].frequency, 220.5);
        QCOMPARE(table->frames[1].amplitude, std::optional(300.0));
        QVERIFY(diagnostics.isEmpty());

        write(pathOf("b_wav.frc"), "FREQ0002");
        QVERIFY(!frc.read(pathOf("b.wav"), 44100, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }

    // A frc read and written again is unchanged, with its reserved bytes. A new frc has the
    // reserved bytes of model4. A map without amplitudes is not written.
    void a_frc_is_written() {
        QDir(m_dir.path()).mkdir(QStringLiteral("x"));
        const auto directory = pathOf("x");
        const FrcFormat frc;
        QVERIFY(frc.canWrite());
        const QByteArray reserved("reserved bytes!!");
        const auto bytes = frcOf(256, 261.6, reserved,
                                 {
                                     {261.6, 1.0},
                                     {0.0,   2.0}
        });
        write(directory / "a_wav.frc", bytes);
        kit::DiagnosticList diagnostics;
        const auto map = frc.read(directory / "a.wav", 44100, diagnostics);
        QVERIFY(map);
        const auto same = frc.changesWriting(directory,
                                             {
                                                 {"a.wav", *map}
        },
                                             diagnostics);
        QVERIFY(same);
        QCOMPARE(same->size(), 1);
        QCOMPARE(same->at(0).path, fs::path("a_wav.frc"));
        QCOMPARE(same->at(0).contents, bytes);

        auto edited = *map;
        edited.frames[1].frequency = 130.8;
        QVERIFY(kit::FrequencyFormats::writeMaps(frc, directory,
                                                 {
                                                     {"b.wav", edited}
        },
                                                 false, diagnostics));
        QCOMPARE(contentsOf(directory / "b_wav.frc"), frcOf(256, 261.6, model4Reserved,
                                                            {
                                                                {261.6, 1.0},
                                                                {130.8, 2.0}
        }));
        QVERIFY(diagnostics.isEmpty());

        edited.frames[0].amplitude.reset();
        QVERIFY(!frc.changesWriting(directory,
                                    {
                                        {"b.wav", edited}
        },
                                    diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }

    // A frc that read() rejects is damaged and not repairable.
    void a_damaged_frc_is_reported() {
        QDir(m_dir.path()).mkdir(QStringLiteral("d"));
        const auto directory = pathOf("d");
        const auto bytes = frcOf(256, 220.0, model4Reserved,
                                 {
                                     {220.0, 1.0}
        });
        write(directory / "a_wav.frc", bytes);
        write(directory / "b_wav.frc", bytes.chopped(1));
        write(directory / "c_wav.frc", frcOf(0, 220.0, model4Reserved,
                                             {
                                                 {220.0, 1.0}
        }));
        const auto damages =
            FrcFormat().checkMapFiles(directory, {"a.wav", "b.wav", "c.wav", "e.wav"});
        QCOMPARE(damages.size(), 2);
        QCOMPARE(damages.at(0).path, fs::path("b_wav.frc"));
        QCOMPARE(damages.at(1).path, fs::path("c_wav.frc"));
        QVERIFY(!damages.at(0).repairable);
        QVERIFY(!damages.at(0).description.isEmpty());
    }
};

QTEST_GUILESS_MAIN(test_FrcFormat)

#include "test_FrcFormat.moc"
