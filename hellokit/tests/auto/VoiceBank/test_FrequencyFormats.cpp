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

    void write(const fs::path &path, const QByteArray &bytes) {
        QFile file(QString::fromStdU16String(path.u16string()));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
    }

    static bool hasMap(const FrequencyFormat &format, const fs::path &wav) {
        return format.mapsExist(wav.parent_path(), {wav.filename()}).value(0);
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

    // A format whose map of a.wav is a.map, with the caches a.env and a.wav.cache
    class Cached : public Fixed {
    public:
        Cached() : Fixed(QStringLiteral("cached")) {
        }

        QList<bool> mapsExist(const fs::path &directory,
                              const QList<fs::path> &wavs) const override {
            return sidecarsExist(directory, wavs);
        }

        std::optional<fs::path> sidecarNameOf(const fs::path &wav) const override {
            return wav.filename().replace_extension(".map");
        }

        QList<AnalysisCache> analysisCaches() const override {
            return {
                {QStringLiteral("ENV"),   AnalysisCache::ReplacedExtension, QStringLiteral(".env")  },
                {QStringLiteral("CACHE"), AnalysisCache::AppendedSuffix,    QStringLiteral(".cache")},
            };
        }
    };

    // A format of Cached whose check reports damages, and whose repair writes "repaired", or
    // removes the map if dropping, as a repair drops a map that is cut short
    class Damaged : public Cached {
    public:
        QList<MapFileDamage> damages;
        bool dropping = false;

        QList<MapFileDamage> checkMapFiles(const fs::path &,
                                           const QList<fs::path> &) const override {
            return damages;
        }

        std::optional<QList<FileChange>> changesRepairing(const fs::path &, const fs::path &path,
                                                          DiagnosticList &) const override {
            return QList<FileChange>{dropping ? FileChange::removed(path)
                                              : FileChange::rewritten(path, "repaired")};
        }
    };

    // A format of Cached whose maps of a directory are in shared.map
    class Shared : public Cached {
    public:
        std::optional<fs::path> mapFileNameOf(const fs::path &) const override {
            return fs::path("shared.map");
        }
    };

    QByteArray contentsOf(const fs::path &path) {
        QFile file(QString::fromStdU16String(path.u16string()));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

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
    // The format of a resampler depends on its file name only, matched whole and without case,
    // with frq as the fallback.
    void a_format_is_chosen_by_the_resampler() {
        FrequencyFormats formats;
        QVERIFY(!formats.formatForResampler("resampler.exe"));
        const BuiltinFrequencyFormats builtins(formats.registry());
        const auto mrq = registration(formats, "mrq", {QStringLiteral("moresampler*.exe")});
        const auto dio = registration(
            formats, "dio", {QStringLiteral("w4u*.exe"), QStringLiteral("world4utau*.exe")});
        QCOMPARE(idsOf(formats), QStringList({"frq", "mrq", "dio"}));
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
        QCOMPARE(changed.size(), 2);
        QCOMPARE(idsOf(formats), QStringList({"sc", "frq"}));
        QCOMPARE(formats.formatForResampler("straycat.exe")->id(), QStringLiteral("sc"));

        const auto frq = registration(formats, "frq");
        QVERIFY(!frq.entry());
        FrequencyFormatRegistry::AddFactory renamed(formats.registry(), "renamed", {}, [] {
            return std::make_unique<Fixed>(QStringLiteral("other"));
        });
        QVERIFY(renamed.entry());
        QCOMPARE(changed.size(), 2);
        QCOMPARE(idsOf(formats), QStringList({"sc", "frq"}));
        QCOMPARE(formats.format(QStringLiteral("frq"))->name(), QStringLiteral("frq (resampler)"));

        auto mrq = registration(formats, "mrq", {QStringLiteral("moresampler*.exe")});
        auto more = registration(formats, "more", {QStringLiteral("moresampler*.exe")});
        QCOMPARE(formats.formatForResampler("moresampler.exe")->id(), QStringLiteral("more"));
        const FrequencyFormats other;
        QVERIFY(other.formats().isEmpty());

        more = {};
        QCOMPARE(formats.formatForResampler("moresampler.exe")->id(), QStringLiteral("mrq"));
        mrq = {};
        builtins.reset();
        QCOMPARE(changed.size(), 7);
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
        QCOMPARE(frq->mapFileNameOf(wav), std::optional<fs::path>("a_wav.frq"));
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

        // The caches beside a map follow it, after the map.
        write(directory / fs::path(u"偁.map"), "map");
        write(directory / fs::path(u"偁.env"), "env");
        write(directory / fs::path(u"偁.wav.cache"), "cache");
        const auto cached = Cached().changesRenaming(directory, renames, diagnostics);
        QVERIFY(cached);
        QCOMPARE(cached->size(), 3);
        QCOMPARE(cached->at(0).path, fs::path(u"偁.map"));
        QCOMPARE(cached->at(0).to, fs::path("b.map"));
        QCOMPARE(cached->at(1).path, fs::path(u"偁.env"));
        QCOMPARE(cached->at(1).to, fs::path("b.env"));
        QCOMPARE(cached->at(2).path, fs::path(u"偁.wav.cache"));
        QCOMPARE(cached->at(2).to, fs::path("b.wav.cache"));
        QVERIFY(diagnostics.isEmpty());
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

    // The caches are declared with the names of their files. frq has none.
    void the_analysis_caches_are_declared() {
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        QVERIFY(formats.format(QStringLiteral("frq"))->analysisCaches().isEmpty());
        const auto caches = Cached().analysisCaches();
        QCOMPARE(caches.at(0).fileNameOf(pathOf("a.wav")), fs::path("a.env"));
        QCOMPARE(caches.at(1).fileNameOf(fs::path(u"あ.wav")), fs::path(u"あ.wav.cache"));
    }

    // Removing the caches keeps the maps, and removing the maps removes their caches.
    void the_caches_are_removed_with_or_without_the_maps() {
        QDir(m_dir.path()).mkdir(QStringLiteral("c"));
        const auto directory = pathOf("c");
        write(directory / "a.map", "map");
        write(directory / "a.env", "env");
        write(directory / "a.wav.cache", "cache");
        const Cached format;

        DiagnosticList diagnostics;
        QVERIFY(FrequencyFormats::invalidateCaches(format, directory, {"a.wav"}, diagnostics));
        QVERIFY(!fs::exists(directory / "a.env"));
        QVERIFY(!fs::exists(directory / "a.wav.cache"));
        QVERIFY(hasMap(format, directory / "a.wav"));

        write(directory / "a.env", "env");
        QVERIFY(FrequencyFormats::removeMaps(format, directory, {"a.wav"}, diagnostics));
        QVERIFY(!fs::exists(directory / "a.env"));
        QVERIFY(!hasMap(format, directory / "a.wav"));
        QVERIFY(diagnostics.isEmpty());
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
    // change. A format that keeps no sidecar files and does not specify how its maps are removed
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

    // A frq read and written again is unchanged, its reserved bytes included. A new frq has zeros
    // there, and the average on the pitch scale if the map records none. A map without
    // amplitudes or with a fractional hop is not written.
    void a_frq_is_written() {
        QDir(m_dir.path()).mkdir(QStringLiteral("w"));
        const auto directory = pathOf("w");
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        const auto frq = formats.format(QStringLiteral("frq"));
        QVERIFY(frq->canWrite());
        QByteArray bytes("FREQ0003");
        bytes += bytesOf<qint32>(128) + bytesOf(220.5) + QByteArray(16, 'x') + bytesOf<qint32>(2);
        bytes += bytesOf(220.0) + bytesOf(0.25) + bytesOf(0.0) + bytesOf(0.5);
        write(directory / "a_wav.frq", bytes);
        DiagnosticList diagnostics;
        const auto map = frq->read(directory / "a.wav", 44100, diagnostics);
        QVERIFY(map);
        QCOMPARE(map->hop, 128.0);
        const auto same = frq->changesWriting(directory,
                                              {
                                                  {"a.wav", *map}
        },
                                              diagnostics);
        QVERIFY(same);
        QCOMPARE(same->size(), 1);
        QCOMPARE(same->at(0).kind, FrequencyFormat::FileChange::Rewrite);
        QCOMPARE(same->at(0).path, fs::path("a_wav.frq"));
        QCOMPARE(same->at(0).contents, bytes);

        auto edited = *map;
        edited.frames[1].frequency = 440;
        edited.averageFrequency.reset();
        QVERIFY(FrequencyFormats::writeMaps(*frq, directory,
                                            {
                                                {"b.wav", edited}
        },
                                            false, diagnostics));
        QByteArray created("FREQ0003");
        created += bytesOf<qint32>(128) + bytesOf(edited.pitchAverage()) + QByteArray(16, '\0') +
                   bytesOf<qint32>(2);
        created += bytesOf(220.0) + bytesOf(0.25) + bytesOf(440.0) + bytesOf(0.5);
        QFile file(QString::fromStdU16String((directory / "b_wav.frq").u16string()));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), created);
        QVERIFY(diagnostics.isEmpty());

        edited.hop = 127.5;
        QVERIFY(!frq->changesWriting(directory,
                                     {
                                         {"b.wav", edited}
        },
                                     diagnostics));
        edited.hop = 128;
        edited.frames[0].amplitude.reset();
        QVERIFY(!frq->changesWriting(directory,
                                     {
                                         {"b.wav", edited}
        },
                                     diagnostics));
        QCOMPARE(diagnostics.size(), 2);
    }

    // The analysis caches are removed with the maps written only if requested. Nothing is
    // changed if a map cannot be written, and a format without writing reports it.
    void the_maps_are_written_with_or_without_their_caches() {
        class Written : public Cached {
        public:
            bool canWrite() const override {
                return true;
            }

            std::optional<QList<FileChange>>
                changesWriting(const fs::path &, const QList<MapWrite> &writes,
                               DiagnosticList &diagnostics) const override {
                QList<FileChange> changes;
                for (const auto &write : writes) {
                    if (write.map.frames.empty()) {
                        return unwritable(write.wav, QStringLiteral("empty"), diagnostics);
                    }
                    changes.push_back(FileChange::rewritten(*sidecarNameOf(write.wav), "map"));
                }
                return changes;
            }
        };
        QDir(m_dir.path()).mkdir(QStringLiteral("m"));
        const auto directory = pathOf("m");
        write(directory / "a.env", "env");
        write(directory / "a.wav.cache", "cache");
        write(directory / "b.env", "env");
        FrequencyMap map;
        map.frames.resize(1);
        const Written written;

        DiagnosticList diagnostics;
        QVERIFY(FrequencyFormats::writeMaps(written, directory,
                                            {
                                                {"a.wav", map}
        },
                                            false, diagnostics));
        QVERIFY(fs::exists(directory / "a.map"));
        QVERIFY(fs::exists(directory / "a.env"));
        QVERIFY(fs::exists(directory / "a.wav.cache"));
        QVERIFY(FrequencyFormats::writeMaps(written, directory,
                                            {
                                                {"a.wav", map},
                                                {"b.wav", map}
        },
                                            true, diagnostics));
        QVERIFY(fs::exists(directory / "b.map"));
        QVERIFY(!fs::exists(directory / "a.env"));
        QVERIFY(!fs::exists(directory / "a.wav.cache"));
        QVERIFY(!fs::exists(directory / "b.env"));
        QVERIFY(diagnostics.isEmpty());

        write(directory / "c.env", "env");
        QVERIFY(!FrequencyFormats::writeMaps(written, directory,
                                             {
                                                 {"c.wav", map},
                                                 {"d.wav", {} }
        },
                                             true, diagnostics));
        QVERIFY(!fs::exists(directory / "c.map"));
        QVERIFY(fs::exists(directory / "c.env"));
        QCOMPARE(diagnostics.size(), 1);

        const Fixed fixed(QStringLiteral("x"));
        QVERIFY(!fixed.canWrite());
        QVERIFY(!FrequencyFormats::writeMaps(fixed, directory,
                                             {
                                                 {"a.wav", map}
        },
                                             false, diagnostics));
        QCOMPARE(diagnostics.size(), 2);
    }

    // A frq that read() rejects is damaged, cannot be repaired, and is reported regardless of the
    // sample rate. A complete frq and an audio file without a frq are not reported.
    void a_damaged_frq_is_reported() {
        QDir(m_dir.path()).mkdir(QStringLiteral("d"));
        const auto directory = pathOf("d");
        QByteArray bytes("FREQ0003");
        bytes += bytesOf<qint32>(256) + bytesOf(220.5) + QByteArray(16, '\0') + bytesOf<qint32>(1);
        bytes += bytesOf(220.0) + bytesOf(22.0);
        write(directory / "a_wav.frq", bytes);
        write(directory / "b_wav.frq", bytes.left(bytes.size() - 1));
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        const auto damages = formats.format(QStringLiteral("frq"))
                                 ->checkMapFiles(directory, {"a.wav", "b.wav", "c.wav"});
        QCOMPARE(damages.size(), 1);
        QCOMPARE(damages.at(0).path, fs::path("b_wav.frq"));
        QVERIFY(!damages.at(0).repairable);
        QVERIFY(!damages.at(0).description.isEmpty());
    }

    // The existing maps come first, a shared file once with all of its audio files, and the
    // caches follow by cache. A damage is attached to its file. A missing shared file is listed,
    // and a missing sidecar file is not.
    void the_map_files_are_listed_with_their_damages() {
        QDir(m_dir.path()).mkdir(QStringLiteral("l"));
        const auto directory = pathOf("l");
        for (const auto name : {"a.map", "b.map", "a.env", "b.env", "b.wav.cache"}) {
            write(directory / name, "x");
        }
        Damaged format;
        format.damages = {
            {"a.map",       QStringLiteral("cut"),    true },
            {"b.wav.cache", QStringLiteral("orphan"), false},
        };
        // c.wav has no map, and its missing sidecar file is not listed.
        const auto files =
            FrequencyFormats::mapFilesOf(format, directory, {"a.wav", "b.wav", "c.wav"});
        QCOMPARE(files.size(), 5);
        QVERIFY(files.at(0).exists);
        QCOMPARE(files.at(0).path, fs::path("a.map"));
        QCOMPARE(files.at(0).cache, -1);
        QCOMPARE(files.at(0).wavs, QList<fs::path>{"a.wav"});
        QVERIFY(files.at(0).damage);
        QVERIFY(files.at(0).damage->repairable);
        QCOMPARE(files.at(1).path, fs::path("b.map"));
        QVERIFY(!files.at(1).damage);
        QCOMPARE(files.at(2).path, fs::path("a.env"));
        QCOMPARE(files.at(2).cache, 0);
        QCOMPARE(files.at(3).path, fs::path("b.env"));
        QCOMPARE(files.at(4).path, fs::path("b.wav.cache"));
        QCOMPARE(files.at(4).cache, 1);
        QCOMPARE(files.at(4).wavs, QList<fs::path>{"b.wav"});
        QCOMPARE(files.at(4).damage->description, QStringLiteral("orphan"));

        // A missing file of several audio files is listed, and the repair skips it.
        const auto missing = FrequencyFormats::mapFilesOf(Shared(), directory, {"a.wav", "b.wav"});
        QCOMPARE(missing.at(0).path, fs::path("shared.map"));
        QVERIFY(!missing.at(0).exists);
        QCOMPARE(missing.at(0).wavs, (QList<fs::path>{"a.wav", "b.wav"}));
        DiagnosticList diagnostics;
        QVERIFY(FrequencyFormats::repairMapFiles(Shared(), directory, {missing.at(0)}, diagnostics)
                    .isEmpty());
        QVERIFY(fs::exists(directory / "a.env"));
        QVERIFY(diagnostics.isEmpty());

        write(directory / "shared.map", "x");
        const auto shared = FrequencyFormats::mapFilesOf(Shared(), directory, {"a.wav", "b.wav"});
        QCOMPARE(shared.at(0).path, fs::path("shared.map"));
        QVERIFY(shared.at(0).exists);
        QCOMPARE(shared.at(0).wavs, (QList<fs::path>{"a.wav", "b.wav"}));
        QCOMPARE(shared.at(1).cache, 0);
    }

    // A repairable file is repaired in place. Another map is removed with the caches of its audio
    // file, and a cache alone, and the audio files of the removed files are returned.
    void the_damaged_files_are_repaired_or_removed() {
        QDir(m_dir.path()).mkdir(QStringLiteral("p"));
        const auto directory = pathOf("p");
        for (const auto name : {"a.map", "b.map", "a.env", "b.env", "b.wav.cache"}) {
            write(directory / name, "x");
        }
        Damaged format;
        format.damages = {
            {"a.map", QStringLiteral("cut"), true},
        };
        const auto files = FrequencyFormats::mapFilesOf(format, directory, {"a.wav", "b.wav"});
        DiagnosticList diagnostics;
        auto again = FrequencyFormats::repairMapFiles(format, directory, {files.at(0), files.at(1)},
                                                      diagnostics);
        QCOMPARE(again, QList<fs::path>{"b.wav"});
        QCOMPARE(contentsOf(directory / "a.map"), QByteArray("repaired"));
        QVERIFY(!fs::exists(directory / "b.map"));
        QVERIFY(!fs::exists(directory / "b.env"));
        QVERIFY(!fs::exists(directory / "b.wav.cache"));
        QVERIFY(fs::exists(directory / "a.env"));

        again = FrequencyFormats::repairMapFiles(format, directory, {files.at(2)}, diagnostics);
        QCOMPARE(again, QList<fs::path>{"a.wav"});
        QVERIFY(!fs::exists(directory / "a.env"));
        QCOMPARE(contentsOf(directory / "a.map"), QByteArray("repaired"));
        QVERIFY(diagnostics.isEmpty());
    }

    // An audio file whose map a repair drops is returned, so that the resampler writes its map
    // again.
    void the_maps_that_a_repair_drops_are_generated_again() {
        QDir(m_dir.path()).mkdir(QStringLiteral("q"));
        const auto directory = pathOf("q");
        write(directory / "a.map", "x");
        Damaged format;
        format.damages = {
            {"a.map", QStringLiteral("cut"), true},
        };
        format.dropping = true;
        const auto files = FrequencyFormats::mapFilesOf(format, directory, {"a.wav"});
        DiagnosticList diagnostics;
        QCOMPARE(FrequencyFormats::repairMapFiles(format, directory, files, diagnostics),
                 QList<fs::path>{"a.wav"});
        QVERIFY(diagnostics.isEmpty());
    }
};

QTEST_GUILESS_MAIN(test_FrequencyFormats)

#include "test_FrequencyFormats.moc"
