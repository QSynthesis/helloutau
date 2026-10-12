#include <filesystem>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QtEndian>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/BuiltinFrequencyFormats.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>

using namespace hello::kit;
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

    static bool hasMap(const FrequencyFormat &format, const fs::path &wav) {
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

    class Fixed : public FrequencyFormat {
    public:
        explicit Fixed(QString id, QStringList patterns = {})
            : m_id(std::move(id)), m_patterns(std::move(patterns)) {
        }

        QString id() const override {
            return m_id;
        }

        QString name() const override {
            return m_id;
        }

        QStringList resamplerPatterns() const override {
            return m_patterns;
        }

        QList<bool> mapsExist(const fs::path &, const QList<fs::path> &wavs) const override {
            return QList<bool>(wavs.size(), false);
        }

        std::optional<FrequencyMap> read(const fs::path &, int, DiagnosticList &) const override {
            return std::nullopt;
        }

    private:
        QString m_id;
        QStringList m_patterns;
    };

    FrequencyFormatRegistry::AddFactory registration(FrequencyFormats &formats, const char *id,
                                                     QStringList patterns = {}) {
        return FrequencyFormatRegistry::AddFactory(formats.registry(), id, {}, [id, patterns] {
            return std::make_unique<Fixed>(QString::fromLatin1(id), patterns);
        });
    }

    QStringList idsOf(const FrequencyFormats &formats) {
        QStringList ids;
        for (const auto format : formats.formats()) {
            ids.push_back(format->id());
        }
        return ids;
    }

}

class test_FrequencyFormats : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    fs::path pathOf(const char *name) const {
        return fs::path(m_dir.path().toStdU16String()) / name;
    }

private Q_SLOTS:
    // The format of a resampler depends on its file name only, with frq as the fallback.
    void a_format_is_chosen_by_the_resampler() {
        FrequencyFormats formats;
        QVERIFY(!formats.formatForResampler("resampler.exe"));
        const BuiltinFrequencyFormats builtins(formats.registry());
        QCOMPARE(idsOf(formats), QStringList({"frq", "dio", "mrq"}));
        const auto idOf = [&formats](const char *resampler) {
            const auto format = formats.formatForResampler(fs::path(resampler));
            return format ? format->id() : QString();
        };
        QCOMPARE(idOf("C:/UTAU/resampler.exe"), QStringLiteral("frq"));
        QCOMPARE(idOf("MoreSampler.EXE"), QStringLiteral("mrq"));
        QCOMPARE(idOf("moresampler-0.8.4.exe"), QStringLiteral("mrq"));
        QCOMPARE(idOf("w4u.exe"), QStringLiteral("dio"));
        QCOMPARE(idOf("world4utau.exe"), QStringLiteral("dio"));
        QCOMPARE(idOf("fresamp14.exe"), QStringLiteral("frq"));
        QCOMPARE(idOf(""), QStringLiteral("frq"));
    }

    // The formats follow the registry in the order of registration, and formatsChanged() is
    // emitted after each change. A second format with a taken ID is not registered, and a
    // format whose ID differs from its name is rejected. Of two formats for one resampler, the
    // later registered is used. Each object has a registry of its own.
    void the_formats_follow_the_registry() {
        FrequencyFormats formats;
        QSignalSpy changed(&formats, &FrequencyFormats::formatsChanged);
        QList<QStringList> seen;
        connect(&formats, &FrequencyFormats::formatsChanged,
                [&] { seen.push_back(idsOf(formats)); });
        auto sc = registration(formats, "sc", {QStringLiteral("straycat*")});
        QCOMPARE(idsOf(formats), QStringList({"sc"}));
        QCOMPARE(seen, QList<QStringList>({{"sc"}}));

        auto builtins = std::make_unique<BuiltinFrequencyFormats>(formats.registry());
        QCOMPARE(changed.size(), 4);
        QCOMPARE(idsOf(formats), QStringList({"sc", "frq", "dio", "mrq"}));
        QCOMPARE(formats.formatForResampler("straycat.exe")->id(), QStringLiteral("sc"));

        const auto frq = registration(formats, "frq");
        QVERIFY(!frq.entry());
        FrequencyFormatRegistry::AddFactory renamed(formats.registry(), "renamed", {}, [] {
            return std::make_unique<Fixed>(QStringLiteral("other"));
        });
        QVERIFY(renamed.entry());
        QCOMPARE(changed.size(), 4);
        QCOMPARE(idsOf(formats), QStringList({"sc", "frq", "dio", "mrq"}));
        QCOMPARE(formats.format(QStringLiteral("frq"))->name(),
                 QStringLiteral("frq (resampler.exe)"));

        auto more = registration(formats, "more", {QStringLiteral("moresampler*.exe")});
        QCOMPARE(formats.formatForResampler("moresampler.exe")->id(), QStringLiteral("more"));
        const FrequencyFormats other;
        QVERIFY(other.formats().isEmpty());

        more = {};
        QCOMPARE(formats.formatForResampler("moresampler.exe")->id(), QStringLiteral("mrq"));
        builtins.reset();
        QCOMPARE(changed.size(), 9);
        QCOMPARE(seen.last(), QStringList({"sc"}));
        QVERIFY(!formats.formatForResampler("resampler.exe"));
    }

    // a_wav.frq: frames every hop samples of the rate of the audio file
    void a_frq_reads() {
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        const auto frq = formats.format(QStringLiteral("frq"));
        const auto wav = pathOf("a.wav");
        QVERIFY(!hasMap(*frq, wav));
        DiagnosticList diagnostics;
        QVERIFY(!frq->read(wav, 44100, diagnostics));
        QCOMPARE(diagnostics.size(), 1);

        QByteArray bytes("FREQ0003");
        bytes += bytesOf<qint32>(256) + bytesOf(220.5) + QByteArray(16, 'x') + bytesOf<qint32>(3);
        for (const double f : {0.0, 220.0, 221.0}) {
            bytes += bytesOf(f) + bytesOf(f / 10);
        }
        write(pathOf("a_wav.frq"), bytes);
        QVERIFY(hasMap(*frq, wav));
        diagnostics.clear();
        const auto table = frq->read(wav, 44100, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->averageFrequency, std::optional<double>(220.5));
        QCOMPARE(table->frames.size(), size_t(3));
        QCOMPARE(table->frames[2].time, 2 * 256 * 1000 / 44100.0);
        QCOMPARE(table->frames[1].frequency, 220.0);
        QCOMPARE(table->frames[2].amplitude, std::optional<double>(22.1));
        QVERIFY(diagnostics.isEmpty());

        // A truncated file and a file of another format
        write(pathOf("a_wav.frq"), bytes.left(bytes.size() - 1));
        QVERIFY(!frq->read(wav, 44100, diagnostics));
        write(pathOf("a_wav.frq"), "FREQ0002");
        QVERIFY(!frq->read(wav, 44100, diagnostics));
        QCOMPARE(diagnostics.size(), 2);
    }

    // b.dio: the time of each frame as written, in seconds
    void a_dio_reads() {
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        const auto dio = formats.format(QStringLiteral("dio"));
        QByteArray bytes("wrld-dio");
        bytes += bytesOf<qint32>(1000) + bytesOf<qint32>(44100) + bytesOf<qint32>(2);
        bytes += bytesOf(0.0) + bytesOf(0.0) + bytesOf(0.005) + bytesOf(261.6);
        write(pathOf("b.dio"), bytes);
        const auto wav = pathOf("b.wav");
        QVERIFY(hasMap(*dio, wav));
        DiagnosticList diagnostics;
        const auto table = dio->read(wav, 44100, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->frames.size(), size_t(2));
        QCOMPARE(table->frames[1].time, 5.0);
        QCOMPARE(table->frames[1].frequency, 261.6);
        QVERIFY(!table->frames[1].amplitude);
        QVERIFY(!table->averageFrequency);
    }

    // An entry of desc.mrq, found by case-insensitive name after a deleted entry
    void an_mrq_entry_reads() {
        QDir(m_dir.path()).mkdir(QStringLiteral("m"));
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        const auto mrq = formats.format(QStringLiteral("mrq"));
        const auto wav = pathOf("m") / fs::path(u"い.wav");
        QVERIFY(!hasMap(*mrq, wav));
        QByteArray bytes("mrq ");
        bytes += bytesOf<qint32>(2) + bytesOf<qint32>(3);
        bytes += mrqEntry(QString(5, QChar(0)), {1.0f});
        bytes += mrqEntry(QStringLiteral("x.wav"), {2.0f});
        bytes += mrqEntry(QStringLiteral("い.WAV"), {0.0f, 440.0f});
        write(pathOf("m") / "desc.mrq", bytes);
        QVERIFY(hasMap(*mrq, wav));
        QVERIFY(!hasMap(*mrq, pathOf("m") / "y.wav"));
        QCOMPARE(mrq->mapsExist(pathOf("m"), {"x.wav", "y.wav", fs::path(u"い.wav")}),
                 (QList<bool>{true, false, true}));
        DiagnosticList diagnostics;
        const auto table = mrq->read(wav, 0, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->frames.size(), size_t(2));
        QCOMPARE(table->frames[1].frequency, 440.0);
        QCOMPARE(table->frames[1].time, 256 * 1000 / 44100.0);
        QVERIFY(!mrq->read(pathOf("m") / "y.wav", 0, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }

    // A map beside its audio file is renamed after it, in one list for all renames of the
    // directory. An audio file without a map needs no change.
    void the_tables_beside_the_audio_files_follow_their_renames() {
        QDir(m_dir.path()).mkdir(QStringLiteral("r"));
        const auto directory = pathOf("r");
        write(directory / "a_wav.frq", "FREQ0003");
        write(directory / fs::path(u"偁.dio"), "wrld-dio");
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        const QList<FrequencyFormat::AudioRename> renames{
            {"a.wav",            fs::path(u"あ.wav")},
            {fs::path(u"偁.wav"), "b.wav"            },
            {"c.wav",            "d.wav"            },
        };

        DiagnosticList diagnostics;
        const auto frq =
            formats.format(QStringLiteral("frq"))->changesRenaming(directory, renames, diagnostics);
        QVERIFY(frq);
        QCOMPARE(frq->size(), 1);
        QCOMPARE(frq->at(0).kind, FrequencyFormat::FileChange::Rename);
        QCOMPARE(frq->at(0).path, fs::path("a_wav.frq"));
        QCOMPARE(frq->at(0).to, fs::path(u"あ_wav.frq"));

        // The caches of world4utau beside the dio follow it.
        write(directory / fs::path(u"偁.star"), "star");
        const auto dio =
            formats.format(QStringLiteral("dio"))->changesRenaming(directory, renames, diagnostics);
        QVERIFY(dio);
        QCOMPARE(dio->size(), 2);
        QCOMPARE(dio->at(0).path, fs::path(u"偁.dio"));
        QCOMPARE(dio->at(0).to, fs::path("b.dio"));
        QCOMPARE(dio->at(1).path, fs::path(u"偁.star"));
        QCOMPARE(dio->at(1).to, fs::path("b.star"));
        QVERIFY(diagnostics.isEmpty());
    }

    // The dio is removed with the caches of world4utau beside it.
    void a_dio_is_removed_with_its_caches() {
        QDir(m_dir.path()).mkdir(QStringLiteral("w"));
        const auto directory = pathOf("w");
        for (const auto name : {"a.dio", "a.star", "a.platinum", "a.wav"}) {
            write(directory / name, "x");
        }
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        DiagnosticList diagnostics;
        QVERIFY(FrequencyFormats::removeMaps(*formats.format(QStringLiteral("dio")), directory,
                                             {"a.wav", "b.wav"}, diagnostics));
        QVERIFY(diagnostics.isEmpty());
        QVERIFY(!fs::exists(directory / "a.dio"));
        QVERIFY(!fs::exists(directory / "a.star"));
        QVERIFY(!fs::exists(directory / "a.platinum"));
        QVERIFY(fs::exists(directory / "a.wav"));
    }

    // A format whose maps cannot be renamed returns the reason, which the caller reports.
    void a_format_without_renames_reports_it() {
        const QList<FrequencyFormat::AudioRename> renames{
            {"a.wav", "b.wav"}
        };
        DiagnosticList diagnostics;
        QVERIFY(!Fixed(QStringLiteral("x")).changesRenaming(pathOf("r"), renames, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);
    }

    // The caches are declared with the names of their files.
    void the_analysis_caches_are_declared() {
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        QVERIFY(formats.format(QStringLiteral("frq"))->analysisCaches().isEmpty());
        const auto dio = formats.format(QStringLiteral("dio"))->analysisCaches();
        QCOMPARE(dio.size(), 2);
        QCOMPARE(dio.at(0).name, QStringLiteral("STAR"));
        QCOMPARE(dio.at(0).fileNameOf(pathOf("a.wav")), fs::path("a.star"));
        QCOMPARE(dio.at(1).fileNameOf("a.wav"), fs::path("a.platinum"));
        const auto mrq = formats.format(QStringLiteral("mrq"))->analysisCaches();
        QCOMPARE(mrq.size(), 1);
        QCOMPARE(mrq.at(0).name, QStringLiteral("LLSM"));
        QCOMPARE(mrq.at(0).fileNameOf(fs::path(u"あ.wav")), fs::path(u"あ.wav.llsm"));
    }

    // Removing the caches keeps the maps, and removing the maps removes their caches.
    void the_caches_are_removed_with_or_without_the_maps() {
        QDir(m_dir.path()).mkdir(QStringLiteral("c"));
        const auto directory = pathOf("c");
        QByteArray bytes("mrq ");
        bytes += bytesOf<qint32>(2) + bytesOf<qint32>(1);
        bytes += mrqEntry(QStringLiteral("a.wav"), {2.0f});
        write(directory / "desc.mrq", bytes);
        write(directory / "a.wav.llsm", "llsm");
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        const auto mrq = formats.format(QStringLiteral("mrq"));

        DiagnosticList diagnostics;
        QVERIFY(FrequencyFormats::invalidateCaches(*mrq, directory, {"a.wav"}, diagnostics));
        QVERIFY(!fs::exists(directory / "a.wav.llsm"));
        QVERIFY(hasMap(*mrq, directory / "a.wav"));

        write(directory / "a.wav.llsm", "llsm");
        QVERIFY(FrequencyFormats::removeMaps(*mrq, directory, {"a.wav"}, diagnostics));
        QVERIFY(!fs::exists(directory / "a.wav.llsm"));
        QVERIFY(!hasMap(*mrq, directory / "a.wav"));
        QVERIFY(diagnostics.isEmpty());
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
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        const auto mrq = formats.format(QStringLiteral("mrq"));
        const QList<FrequencyFormat::AudioRename> renames{
            {"a.wav", "x.wav"},
            {"x.wav", "a.wav"},
            {"b.wav", "c.wav"},
        };

        DiagnosticList diagnostics;
        const auto changes = mrq->changesRenaming(directory, renames, diagnostics);
        QVERIFY(changes);
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(changes->size(), 2);
        QCOMPARE(changes->at(0).kind, FrequencyFormat::FileChange::Rename);
        QCOMPARE(changes->at(0).path, fs::path("a.wav.llsm"));
        QCOMPARE(changes->at(0).to, fs::path("x.wav.llsm"));
        QCOMPARE(changes->at(1).kind, FrequencyFormat::FileChange::Rewrite);
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

    // A format of a plugin that names the sidecar file of each audio file follows renames without
    // more code.
    void a_format_that_names_its_tables_follows_renames() {
        class Pmk : public Fixed {
        public:
            Pmk() : Fixed(QStringLiteral("pmk")) {
            }

            std::optional<fs::path> sidecarNameOf(const fs::path &wav) const override {
                return wav.filename().replace_extension(".pmk");
            }
        };
        QDir(m_dir.path()).mkdir(QStringLiteral("p"));
        write(pathOf("p") / "a.pmk", "pmk");
        const QList<FrequencyFormat::AudioRename> renames{
            {"a.wav", "b.wav"},
            {"c.wav", "d.wav"},
        };
        DiagnosticList diagnostics;
        const auto changes = Pmk().changesRenaming(pathOf("p"), renames, diagnostics);
        QVERIFY(changes);
        QCOMPARE(changes->size(), 1);
        QCOMPARE(changes->at(0).path, fs::path("a.pmk"));
        QCOMPARE(changes->at(0).to, fs::path("b.pmk"));
        QVERIFY(diagnostics.isEmpty());
    }

    void the_changes_are_made_by_their_kinds() {
        const auto renamed = FrequencyFormat::FileChange::renamed("a_wav.frq", "b_wav.frq");
        QCOMPARE(renamed.kind, FrequencyFormat::FileChange::Rename);
        QCOMPARE(renamed.path, fs::path("a_wav.frq"));
        QCOMPARE(renamed.to, fs::path("b_wav.frq"));
        const auto rewritten = FrequencyFormat::FileChange::rewritten("desc.mrq", "mrq ");
        QCOMPARE(rewritten.kind, FrequencyFormat::FileChange::Rewrite);
        QCOMPARE(rewritten.path, fs::path("desc.mrq"));
        QCOMPARE(rewritten.contents, QByteArray("mrq "));
        const auto removed = FrequencyFormat::FileChange::removed("a_wav.frq");
        QCOMPARE(removed.kind, FrequencyFormat::FileChange::Remove);
        QCOMPARE(removed.path, fs::path("a_wav.frq"));
    }

    // The maps beside their audio files are removed, and an audio file without a map needs no
    // change. A format that keeps no sidecar files and does not say how its maps are removed
    // reports it.
    void the_maps_beside_the_audio_files_are_removed() {
        QDir(m_dir.path()).mkdir(QStringLiteral("d"));
        const auto directory = pathOf("d");
        write(directory / "a_wav.frq", "FREQ0003");
        write(directory / "a.wav", "RIFF");
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        const auto frq = formats.format(QStringLiteral("frq"));

        DiagnosticList diagnostics;
        const auto changes = frq->changesRemoving(directory, {"a.wav", "b.wav"}, diagnostics);
        QVERIFY(changes);
        QCOMPARE(changes->size(), 1);
        QCOMPARE(changes->at(0).kind, FrequencyFormat::FileChange::Remove);
        QCOMPARE(changes->at(0).path, fs::path("a_wav.frq"));

        QVERIFY(FrequencyFormats::removeMaps(*frq, directory, {"a.wav", "b.wav"}, diagnostics));
        QVERIFY(diagnostics.isEmpty());
        QVERIFY(!fs::exists(directory / "a_wav.frq"));
        QVERIFY(fs::exists(directory / "a.wav"));

        QVERIFY(!FrequencyFormats::removeMaps(Fixed(QStringLiteral("x")), directory, {"a.wav"},
                                              diagnostics));
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);
    }

    // The entries of desc.mrq are deleted as the mrq library deletes them, their names zeroed in
    // place, whatever the case of the names. The other entries stay readable. A folder without
    // desc.mrq needs no change.
    void the_entries_of_desc_mrq_are_removed() {
        QDir(m_dir.path()).mkdir(QStringLiteral("m"));
        const auto directory = pathOf("m");
        QByteArray bytes("mrq ");
        bytes += bytesOf<qint32>(2) + bytesOf<qint32>(3);
        bytes += mrqEntry(QStringLiteral("x.wav"), {2.0f});
        bytes += mrqEntry(QStringLiteral("A.WAV"), {0.0f, 440.0f});
        bytes += mrqEntry(QStringLiteral("a.wav"), {1.0f});
        write(directory / "desc.mrq", bytes);
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        const auto mrq = formats.format(QStringLiteral("mrq"));

        DiagnosticList diagnostics;
        QVERIFY(FrequencyFormats::removeMaps(*mrq, directory, {"a.wav", "b.wav"}, diagnostics));
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
        QCOMPARE(mrq->mapsExist(directory, {"x.wav", "a.wav"}), (QList<bool>{true, false}));
        const auto table = mrq->read(directory / "x.wav", 0, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->frames.at(0).frequency, 2.0);

        const auto changes = mrq->changesRemoving(pathOf("none"), {"a.wav"}, diagnostics);
        QVERIFY(changes);
        QVERIFY(changes->isEmpty());
    }
};

QTEST_GUILESS_MAIN(test_FrequencyFormats)

#include "test_FrequencyFormats.moc"
