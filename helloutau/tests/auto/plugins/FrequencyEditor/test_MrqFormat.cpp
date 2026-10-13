#include <filesystem>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QtEndian>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/FrequencyFormats.h>

#include <FrequencyEditor/MrqFormat.h>

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

    QByteArray utf16(const QString &text) {
        QByteArray bytes;
        for (const auto c : text) {
            bytes += bytesOf<quint16>(c.unicode());
        }
        return bytes;
    }

    void write(const fs::path &path, const QByteArray &bytes) {
        QFile file(QString::fromStdU16String(path.u16string()));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
    }

    bool hasMap(const kit::FrequencyFormat &format, const fs::path &wav) {
        return format.mapsExist(wav.parent_path(), {wav.filename()}).value(0);
    }

    // An entry of desc.mrq of version 2
    QByteArray mrqEntry(const QString &name, const QList<float> &frequencies, qint32 rate = 44100,
                        qint32 timestamp = 0, qint32 flag = 1) {
        QByteArray data = bytesOf<qint32>(qint32(frequencies.size())) + bytesOf<qint32>(rate) +
                          bytesOf<qint32>(256);
        for (const float f : frequencies) {
            data += bytesOf(f);
        }
        data += bytesOf<qint32>(timestamp) + bytesOf<qint32>(flag);
        return bytesOf<qint32>(qint32(name.size())) + utf16(name) +
               bytesOf<qint32>(qint32(data.size())) + data;
    }

    // The timestamp of the last entry of contents
    qint32 lastTimestamp(const QByteArray &contents) {
        return qFromLittleEndian<qint32>(contents.constData() + contents.size() - 8);
    }

    kit::FrequencyMap mapOf(const QList<double> &frequencies) {
        kit::FrequencyMap map;
        map.hop = 256;
        for (const double f : frequencies) {
            kit::FrequencyMap::Frame frame;
            frame.frequency = f;
            map.frames.push_back(frame);
        }
        return map;
    }

}

class test_MrqFormat : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    fs::path pathOf(const char *name) const {
        return fs::path(m_dir.path().toStdU16String()) / name;
    }

private Q_SLOTS:
    // The format of moresampler and its cache
    void the_format_is_declared() {
        const MrqFormat mrq;
        QCOMPARE(mrq.id(), QStringLiteral("mrq"));
        QCOMPARE(mrq.resamplerPatterns(), QStringList{QStringLiteral("moresampler*.exe")});
        QCOMPARE(mrq.mapFileNameOf(fs::path(u"あ.wav")), std::optional<fs::path>("desc.mrq"));
        const auto caches = mrq.analysisCaches();
        QCOMPARE(caches.size(), 1);
        QCOMPARE(caches.at(0).name, QStringLiteral("LLSM"));
        QCOMPARE(caches.at(0).fileNameOf(fs::path(u"あ.wav")), fs::path(u"あ.wav.llsm"));
    }

    // An entry of desc.mrq, found by case-insensitive name after a deleted entry
    void an_mrq_entry_reads() {
        QDir(m_dir.path()).mkdir(QStringLiteral("m"));
        const MrqFormat mrq;
        const auto wav = pathOf("m") / fs::path(u"い.wav");
        QVERIFY(!hasMap(mrq, wav));
        QByteArray bytes("mrq ");
        bytes += bytesOf<qint32>(2) + bytesOf<qint32>(3);
        bytes += mrqEntry(QString(5, QChar(0)), {1.0f});
        bytes += mrqEntry(QStringLiteral("x.wav"), {2.0f});
        bytes += mrqEntry(QStringLiteral("い.WAV"), {0.0f, 440.0f});
        write(pathOf("m") / "desc.mrq", bytes);
        QVERIFY(hasMap(mrq, wav));
        QVERIFY(!hasMap(mrq, pathOf("m") / "y.wav"));
        QCOMPARE(mrq.mapsExist(pathOf("m"), {"x.wav", "y.wav", fs::path(u"い.wav")}),
                 (QList<bool>{true, false, true}));
        kit::DiagnosticList diagnostics;
        const auto table = mrq.read(wav, 0, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->frames.size(), size_t(2));
        QCOMPARE(table->frames[1].frequency, 440.0);
        QCOMPARE(table->frames[1].time, 256 * 1000 / 44100.0);
        QVERIFY(!mrq.read(pathOf("m") / "y.wav", 0, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }

    // The entries of renamed audio files are appended under the new names, whose old names and
    // an entry under a new name are zeroed, and the LLSM files are renamed. Two audio files
    // exchange their entries.
    void the_entries_of_desc_mrq_follow_their_renames() {
        QDir(m_dir.path()).mkdir(QStringLiteral("n"));
        const auto directory = pathOf("n");
        QByteArray bytes("mrq ");
        bytes += bytesOf<qint32>(2) + bytesOf<qint32>(3);
        bytes += mrqEntry(QStringLiteral("a.wav"), {1.0f});
        bytes += mrqEntry(QStringLiteral("x.wav"), {2.0f});
        bytes += mrqEntry(QStringLiteral("C.WAV"), {3.0f});
        write(directory / "desc.mrq", bytes);
        write(directory / "a.wav.llsm", "llsm");
        const QList<kit::FrequencyFormat::AudioRename> renames{
            {"a.wav", "x.wav"},
            {"x.wav", "a.wav"},
            {"b.wav", "c.wav"},
        };

        kit::DiagnosticList diagnostics;
        const auto changes = MrqFormat().changesRenaming(directory, renames, diagnostics);
        QVERIFY(changes);
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(changes->size(), 2);
        QCOMPARE(changes->at(0).kind, kit::FrequencyFormat::FileChange::Rename);
        QCOMPARE(changes->at(0).path, fs::path("a.wav.llsm"));
        QCOMPARE(changes->at(0).to, fs::path("x.wav.llsm"));
        QCOMPARE(changes->at(1).kind, kit::FrequencyFormat::FileChange::Rewrite);
        QCOMPARE(changes->at(1).path, fs::path("desc.mrq"));

        QByteArray expected("mrq ");
        expected += bytesOf<qint32>(2) + bytesOf<qint32>(5);
        expected += mrqEntry(QString(5, QChar(0)), {1.0f});
        expected += mrqEntry(QString(5, QChar(0)), {2.0f});
        expected += mrqEntry(QString(5, QChar(0)), {3.0f});
        expected += mrqEntry(QStringLiteral("x.wav"), {1.0f});
        expected += mrqEntry(QStringLiteral("a.wav"), {2.0f});
        QCOMPARE(changes->at(1).contents, expected);
    }

    // The entries of desc.mrq are deleted as the mrq library deletes them, their names zeroed in
    // place, whatever the case of the names, with their LLSM files. The other entries stay
    // readable. A folder without desc.mrq needs no change.
    void the_entries_of_desc_mrq_are_removed() {
        QDir(m_dir.path()).mkdir(QStringLiteral("m"));
        const auto directory = pathOf("m");
        QByteArray bytes("mrq ");
        bytes += bytesOf<qint32>(2) + bytesOf<qint32>(3);
        bytes += mrqEntry(QStringLiteral("x.wav"), {2.0f});
        bytes += mrqEntry(QStringLiteral("A.WAV"), {0.0f, 440.0f});
        bytes += mrqEntry(QStringLiteral("a.wav"), {1.0f});
        write(directory / "desc.mrq", bytes);
        write(directory / "a.wav.llsm", "llsm");
        write(directory / "x.wav.llsm", "llsm");
        const MrqFormat mrq;

        kit::DiagnosticList diagnostics;
        QVERIFY(kit::FrequencyFormats::removeMaps(mrq, directory, {"a.wav", "b.wav"}, diagnostics));
        QVERIFY(diagnostics.isEmpty());
        QByteArray deleted("mrq ");
        deleted += bytesOf<qint32>(2) + bytesOf<qint32>(3);
        deleted += mrqEntry(QStringLiteral("x.wav"), {2.0f});
        deleted += mrqEntry(QString(5, QChar(0)), {0.0f, 440.0f});
        deleted += mrqEntry(QString(5, QChar(0)), {1.0f});
        QFile file(QString::fromStdU16String((directory / "desc.mrq").u16string()));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), deleted);
        file.close();
        QVERIFY(!fs::exists(directory / "a.wav.llsm"));
        QVERIFY(fs::exists(directory / "x.wav.llsm"));
        QCOMPARE(mrq.mapsExist(directory, {"x.wav", "a.wav"}), (QList<bool>{true, false}));
        const auto table = mrq.read(directory / "x.wav", 0, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->frames.at(0).frequency, 2.0);

        const auto changes = mrq.changesRemoving(pathOf("none"), {"a.wav"}, diagnostics);
        QVERIFY(changes);
        QVERIFY(changes->isEmpty());
    }

    // Removing the LLSM file alone keeps the entry, from which moresampler writes it again.
    void the_cache_is_removed_without_the_entry() {
        QDir(m_dir.path()).mkdir(QStringLiteral("c"));
        const auto directory = pathOf("c");
        QByteArray bytes("mrq ");
        bytes += bytesOf<qint32>(2) + bytesOf<qint32>(1);
        bytes += mrqEntry(QStringLiteral("a.wav"), {2.0f});
        write(directory / "desc.mrq", bytes);
        write(directory / "a.wav.llsm", "llsm");
        const MrqFormat mrq;
        kit::DiagnosticList diagnostics;
        QVERIFY(kit::FrequencyFormats::invalidateCaches(mrq, directory, {"a.wav"}, diagnostics));
        QVERIFY(!fs::exists(directory / "a.wav.llsm"));
        QVERIFY(hasMap(mrq, directory / "a.wav"));
        QVERIFY(diagnostics.isEmpty());
    }

    // An entry with room for the frames is overwritten in place with the time of writing and the
    // flag of an edit by hand. A longer entry has its name zeroed and is appended.
    void the_entries_of_desc_mrq_are_written() {
        QDir(m_dir.path()).mkdir(QStringLiteral("w"));
        const auto directory = pathOf("w");
        QByteArray bytes("mrq ");
        bytes += bytesOf<qint32>(2) + bytesOf<qint32>(2);
        bytes += mrqEntry(QStringLiteral("x.wav"), {2.0f});
        bytes += mrqEntry(QStringLiteral("a.wav"), {1.0f, 3.0f});
        write(directory / "desc.mrq", bytes);
        const MrqFormat mrq;
        QVERIFY(mrq.canWrite());
        kit::DiagnosticList diagnostics;
        const auto map = mrq.read(directory / "a.wav", 44100, diagnostics);
        QVERIFY(map);
        QCOMPARE(map->hop, 256.0);

        auto edited = *map;
        edited.frames[0].frequency = 440;
        const auto before = QDateTime::currentSecsSinceEpoch();
        const auto changes = mrq.changesWriting(directory,
                                                {
                                                    {"a.wav", edited}
        },
                                                diagnostics);
        QVERIFY(changes);
        QCOMPARE(changes->size(), 1);
        QCOMPARE(changes->at(0).path, fs::path("desc.mrq"));
        const auto timestamp = lastTimestamp(changes->at(0).contents);
        QVERIFY(timestamp >= before && timestamp <= QDateTime::currentSecsSinceEpoch());
        QByteArray expected("mrq ");
        expected += bytesOf<qint32>(2) + bytesOf<qint32>(2);
        expected += mrqEntry(QStringLiteral("x.wav"), {2.0f});
        expected += mrqEntry(QStringLiteral("a.wav"), {440.0f, 3.0f}, 44100, timestamp, 0);
        QCOMPARE(changes->at(0).contents, expected);

        const auto longer = mrq.changesWriting(directory,
                                               {
                                                   {"a.wav", mapOf({1, 2, 3}), 44100, false}
        },
                                               diagnostics);
        QVERIFY(longer);
        expected = "mrq ";
        expected += bytesOf<qint32>(2) + bytesOf<qint32>(3);
        expected += mrqEntry(QStringLiteral("x.wav"), {2.0f});
        expected += mrqEntry(QString(5, QChar(0)), {1.0f, 3.0f});
        expected += mrqEntry(QStringLiteral("a.wav"), {1.0f, 2.0f, 3.0f}, 44100,
                             lastTimestamp(longer->at(0).contents), 1);
        QCOMPARE(longer->at(0).contents, expected);
        QVERIFY(diagnostics.isEmpty());
    }

    // An entry of version 1 has no room for the timestamp and is appended, and the file becomes
    // version 2. A new desc.mrq takes the sample rate of the audio file, which must be known.
    void desc_mrq_is_written_in_version_2() {
        QDir(m_dir.path()).mkdir(QStringLiteral("v"));
        const auto directory = pathOf("v");
        QByteArray entry =
            bytesOf<qint32>(1) + bytesOf<qint32>(44100) + bytesOf<qint32>(256) + bytesOf(1.0f);
        QByteArray bytes("mrq ");
        bytes += bytesOf<qint32>(1) + bytesOf<qint32>(1) + bytesOf<qint32>(5) +
                 utf16(QStringLiteral("a.wav")) + bytesOf<qint32>(qint32(entry.size())) + entry;
        write(directory / "desc.mrq", bytes);
        const MrqFormat mrq;
        kit::DiagnosticList diagnostics;
        const auto changes = mrq.changesWriting(directory,
                                                {
                                                    {"a.wav", mapOf({1}), 0}
        },
                                                diagnostics);
        QVERIFY(changes);
        QByteArray expected("mrq ");
        expected += bytesOf<qint32>(2) + bytesOf<qint32>(2) + bytesOf<qint32>(5) +
                    QByteArray(10, '\0') + bytesOf<qint32>(qint32(entry.size())) + entry;
        expected += mrqEntry(QStringLiteral("a.wav"), {1.0f}, 44100,
                             lastTimestamp(changes->at(0).contents), 0);
        QCOMPARE(changes->at(0).contents, expected);

        const auto created = mrq.changesWriting(pathOf("none"),
                                                {
                                                    {"b.wav", mapOf({2}), 48000}
        },
                                                diagnostics);
        QVERIFY(created);
        expected = "mrq ";
        expected += bytesOf<qint32>(2) + bytesOf<qint32>(1);
        expected += mrqEntry(QStringLiteral("b.wav"), {2.0f}, 48000,
                             lastTimestamp(created->at(0).contents), 0);
        QCOMPARE(created->at(0).contents, expected);
        QVERIFY(diagnostics.isEmpty());

        QVERIFY(!mrq.changesWriting(pathOf("none"),
                                    {
                                        {"b.wav", mapOf({2}), 0}
        },
                                    diagnostics));
        auto fractional = mapOf({2});
        fractional.hop = 256.5;
        QVERIFY(!mrq.changesWriting(directory,
                                    {
                                        {"a.wav", fractional}
        },
                                    diagnostics));
        QCOMPARE(diagnostics.size(), 2);
    }
};

QTEST_GUILESS_MAIN(test_MrqFormat)

#include "test_MrqFormat.moc"
