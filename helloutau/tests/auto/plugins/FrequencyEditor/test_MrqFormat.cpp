#include <filesystem>

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
    QByteArray mrqEntry(const QString &name, const QList<float> &frequencies) {
        QByteArray data = bytesOf<qint32>(qint32(frequencies.size())) + bytesOf<qint32>(44100) +
                          bytesOf<qint32>(256);
        for (const float f : frequencies) {
            data += bytesOf(f);
        }
        data += bytesOf<qint32>(0) + bytesOf<qint32>(1);
        return bytesOf<qint32>(qint32(name.size())) + utf16(name) +
               bytesOf<qint32>(qint32(data.size())) + data;
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
};

QTEST_GUILESS_MAIN(test_MrqFormat)

#include "test_MrqFormat.moc"
