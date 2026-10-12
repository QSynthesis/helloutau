#include "BuiltinFrequencyFormats.h"

#include <algorithm>
#include <cstring>

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QSet>
#include <QtCore/QtEndian>

#include "FrequencyFormat.h"

namespace hello::kit {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        bool fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message, std::nullopt});
            return false;
        }

        std::optional<QByteArray> contentsOf(const std::filesystem::path &path,
                                             DiagnosticList &diagnostics) {
            QFile file(textOf(path));
            if (!file.open(QIODevice::ReadOnly)) {
                fail(diagnostics,
                     FrequencyFormat::tr("\"%1\" could not be read.").arg(textOf(path)));
                return std::nullopt;
            }
            return file.readAll();
        }

        // Reads little-endian values from bytes, each only if the bytes hold it.
        class Reader {
        public:
            explicit Reader(const QByteArray &bytes, qsizetype at = 0) : m_bytes(bytes), m_at(at) {
            }

            qsizetype at() const {
                return m_at;
            }

            qsizetype left() const {
                return m_bytes.size() - m_at;
            }

            bool skip(qsizetype count) {
                if (count < 0 || count > left()) {
                    return false;
                }
                m_at += count;
                return true;
            }

            template <class T>
            std::optional<T> next() {
                if (left() < qsizetype(sizeof(T))) {
                    return std::nullopt;
                }
                const auto value = qFromLittleEndian<T>(m_bytes.constData() + m_at);
                m_at += qsizetype(sizeof(T));
                return value;
            }

        private:
            const QByteArray &m_bytes;
            qsizetype m_at;
        };

        // Whether each audio file of wavs in directory has the sidecar file that format names
        QList<bool> sidecarsExist(const FrequencyFormat &format,
                                  const std::filesystem::path &directory,
                                  const QList<std::filesystem::path> &wavs) {
            QList<bool> found;
            for (const auto &wav : wavs) {
                std::error_code error;
                found.push_back(std::filesystem::is_regular_file(
                    directory / *format.sidecarNameOf(wav), error));
            }
            return found;
        }

        std::optional<FrequencyMap> damaged(const std::filesystem::path &path,
                                              DiagnosticList &diagnostics) {
            fail(diagnostics,
                 FrequencyFormat::tr("\"%1\" is no frequency map of its format, or it is cut "
                                     "short.")
                     .arg(textOf(path)));
            return std::nullopt;
        }

        // The frq of resampler.exe, beside the audio file as a_wav.frq. The layout, as the frq
        // reader of OpenUtau reads it
        // (https://github.com/stakira/OpenUtau/blob/0.1.565/OpenUtau.Core/Classic/Frq.cs, and the
        // name in GetFrqFile() of VoicebankFiles.cs of the same tag): FREQ0003, the int32 hop in
        // samples, the double average frequency, 16 reserved bytes, the int32 frame count, and
        // each frame a double frequency and a double amplitude.
        class Frq : public FrequencyFormat {
        public:
            QString id() const override {
                return QStringLiteral("frq");
            }

            QString name() const override {
                return tr("frq (resampler.exe)");
            }

            QStringList resamplerPatterns() const override {
                return {QStringLiteral("resampler*.exe")};
            }

            QList<bool> mapsExist(const std::filesystem::path &directory,
                                  const QList<std::filesystem::path> &wavs) const override {
                return sidecarsExist(*this, directory, wavs);
            }

            std::optional<FrequencyMap> read(const std::filesystem::path &wav, int sampleRate,
                                               DiagnosticList &diagnostics) const override {
                const auto path = pathOf(wav);
                const auto bytes = contentsOf(path, diagnostics);
                if (!bytes) {
                    return std::nullopt;
                }
                if (!bytes->startsWith("FREQ0003")) {
                    return damaged(path, diagnostics);
                }
                Reader reader(*bytes, 8);
                const auto hop = reader.next<qint32>();
                const auto average = reader.next<double>();
                if (!hop || !average || !reader.skip(16) || *hop <= 0 || sampleRate <= 0) {
                    return damaged(path, diagnostics);
                }
                const auto count = reader.next<qint32>();
                if (!count || *count < 0 || reader.left() < qsizetype(*count) * 16) {
                    return damaged(path, diagnostics);
                }
                FrequencyMap table;
                table.averageFrequency = *average;
                table.frames.reserve(size_t(*count));
                for (qint32 i = 0; i < *count; ++i) {
                    FrequencyMap::Frame frame;
                    frame.time = double(i) * *hop * 1000 / sampleRate;
                    frame.frequency = *reader.next<double>();
                    frame.amplitude = *reader.next<double>();
                    table.frames.push_back(frame);
                }
                return table;
            }

            std::optional<std::filesystem::path>
                sidecarNameOf(const std::filesystem::path &wav) const override {
                auto name = wav.stem().u16string();
                auto extension = wav.extension().u16string();
                for (auto &c : extension) {
                    if (c == u'.') {
                        c = u'_';
                    }
                }
                return std::filesystem::path(name + extension + u".frq");
            }

        private:
            std::filesystem::path pathOf(const std::filesystem::path &wav) const {
                return wav.parent_path() / *sidecarNameOf(wav);
            }
        };

        // The dio of world4utau, beside the audio file as a.dio. The layout, as world4utau
        // writes it (makeFilename() and the writing of the .dio file in src/world4utau.cpp,
        // https://github.com/LucasCTN/world4utau/blob/2b48d4cc1e348b1a12c32134dbbd8e7b8e2818ef/src/world4utau.cpp):
        // wrld-dio, the int32 signal length, the int32 sample rate, the int32 frame count, and
        // each frame a double time in seconds and a double frequency, 0 where unvoiced.
        class Dio : public FrequencyFormat {
        public:
            QString id() const override {
                return QStringLiteral("dio");
            }

            QString name() const override {
                return tr("dio (world4utau)");
            }

            QStringList resamplerPatterns() const override {
                return {QStringLiteral("w4u*.exe"), QStringLiteral("world4utau*.exe")};
            }

            QList<bool> mapsExist(const std::filesystem::path &directory,
                                  const QList<std::filesystem::path> &wavs) const override {
                return sidecarsExist(*this, directory, wavs);
            }

            std::optional<FrequencyMap> read(const std::filesystem::path &wav, int sampleRate,
                                               DiagnosticList &diagnostics) const override {
                Q_UNUSED(sampleRate);
                const auto path = pathOf(wav);
                const auto bytes = contentsOf(path, diagnostics);
                if (!bytes) {
                    return std::nullopt;
                }
                if (!bytes->startsWith("wrld-dio")) {
                    return damaged(path, diagnostics);
                }
                Reader reader(*bytes, 8);
                const auto signalLength = reader.next<qint32>();
                const auto rate = reader.next<qint32>();
                const auto count = reader.next<qint32>();
                if (!signalLength || !rate || !count || *count < 0 ||
                    reader.left() < qsizetype(*count) * 16) {
                    return damaged(path, diagnostics);
                }
                FrequencyMap table;
                table.frames.reserve(size_t(*count));
                for (qint32 i = 0; i < *count; ++i) {
                    FrequencyMap::Frame frame;
                    frame.time = *reader.next<double>() * 1000;
                    frame.frequency = *reader.next<double>();
                    table.frames.push_back(frame);
                }
                return table;
            }

            std::optional<std::filesystem::path>
                sidecarNameOf(const std::filesystem::path &wav) const override {
                return wav.filename().replace_extension(".dio");
            }

            // The spectral envelope and the aperiodicity, which world4utau writes beside the map
            // as a.star and a.platinum (writeSTARParam() and writePlatinumParam() in the source
            // above)
            QList<AnalysisCache> analysisCaches() const override {
                return {
                    {QStringLiteral("STAR"),     AnalysisCache::ReplacedExtension,
                     QStringLiteral(".star")    },
                    {QStringLiteral("PLATINUM"), AnalysisCache::ReplacedExtension,
                     QStringLiteral(".platinum")},
                };
            }

        private:
            std::filesystem::path pathOf(const std::filesystem::path &wav) const {
                return wav.parent_path() / *sidecarNameOf(wav);
            }
        };

        // The mrq of moresampler: an entry named after the audio file in desc.mrq of its folder.
        // The layout, as the mrq library of the author of moresampler reads it
        // (https://github.com/Sleepwalking/mrq/blob/6b2df0d5965bc4dce8c99be6a3eb1325ba94718e/mrq.h
        // and mrq.c): "mrq ", the int32 version and the int32 entry count; each entry the int32
        // length of its name, the name in UTF-16, the int32 size of its data, then the int32
        // frame count, the int32 sample rate, the int32 hop in samples, a float frequency per
        // frame, and in version 2 a timestamp and a flag. A deleted entry keeps its place with
        // its name zeroed.
        class Mrq : public FrequencyFormat {
        public:
            QString id() const override {
                return QStringLiteral("mrq");
            }

            QString name() const override {
                return tr("mrq (moresampler)");
            }

            QStringList resamplerPatterns() const override {
                return {QStringLiteral("moresampler*.exe")};
            }

            // desc.mrq is read once, and each name is looked up as find() looks it up.
            QList<bool> mapsExist(const std::filesystem::path &directory,
                                  const QList<std::filesystem::path> &wavs) const override {
                DiagnosticList ignored;
                const auto bytes = contentsOf(directory / "desc.mrq", ignored);
                QSet<QString> names;
                if (bytes) {
                    for (const auto &entry : entriesOf(*bytes)) {
                        names.insert(entry.first.toCaseFolded());
                    }
                }
                QList<bool> found;
                for (const auto &wav : wavs) {
                    found.push_back(names.contains(textOf(wav.filename()).toCaseFolded()));
                }
                return found;
            }

            std::optional<FrequencyMap> read(const std::filesystem::path &wav, int sampleRate,
                                               DiagnosticList &diagnostics) const override {
                Q_UNUSED(sampleRate);
                const auto path = wav.parent_path() / "desc.mrq";
                const auto bytes = contentsOf(path, diagnostics);
                if (!bytes) {
                    return std::nullopt;
                }
                const auto at = find(*bytes, wav);
                if (!at) {
                    fail(diagnostics, tr("\"%1\" has no entry for \"%2\".")
                                          .arg(textOf(path), textOf(wav.filename())));
                    return std::nullopt;
                }
                Reader reader(*bytes, *at);
                const auto count = reader.next<qint32>();
                const auto rate = reader.next<qint32>();
                const auto hop = reader.next<qint32>();
                if (!count || !rate || !hop || *count < 0 || *rate <= 0 || *hop <= 0 ||
                    reader.left() < qsizetype(*count) * 4) {
                    return damaged(path, diagnostics);
                }
                FrequencyMap table;
                table.frames.reserve(size_t(*count));
                for (qint32 i = 0; i < *count; ++i) {
                    FrequencyMap::Frame frame;
                    frame.time = double(i) * *hop * 1000 / *rate;
                    frame.frequency = *reader.next<float>();
                    table.frames.push_back(frame);
                }
                return table;
            }

            // The analysis of moresampler beside the audio file as a.wav.llsm, which moresampler
            // derives from the entry and writes again if missing, measured with moresampler
            // 0.8.4 on 2026-10-12 (docs/FrequencyMaps.md). Its readme states that desc.mrq is
            // read before an LLSM file is created, so that a deleted entry is written again only
            // if the LLSM file is missing as well.
            QList<AnalysisCache> analysisCaches() const override {
                return {
                    {QStringLiteral("LLSM"), AnalysisCache::AppendedSuffix,
                     QStringLiteral(".llsm")},
                };
            }

            // The entry of each renamed audio file is written again under the new name, as
            // mrq_write_entry() in mrq.c writes an entry that does not fit: the old name is zeroed
            // and the entry is appended. moresampler names an entry after the audio file in
            // UTF-16 (measured as above). An entry under the new name is deleted as well, so that
            // the appended entry is the only one.
            std::optional<QList<FileChange>>
                changesRenaming(const std::filesystem::path &directory,
                                const QList<AudioRename> &renames,
                                DiagnosticList &diagnostics) const override {
                auto changes = cacheRenames(directory, renames);
                const auto path = directory / "desc.mrq";
                std::error_code error;
                if (!std::filesystem::is_regular_file(path, error)) {
                    return changes;
                }
                auto bytes = contentsOf(path, diagnostics);
                if (!bytes) {
                    return std::nullopt;
                }
                // The data of every renamed entry is taken before any name is zeroed, so that two
                // audio files may exchange their names.
                QByteArray appended;
                qint32 added = 0;
                QSet<QString> zeroed;
                for (const auto &rename : renames) {
                    const auto at = find(*bytes, rename.from);
                    zeroed.insert(textOf(rename.from.filename()).toCaseFolded());
                    zeroed.insert(textOf(rename.to.filename()).toCaseFolded());
                    if (!at) {
                        continue;
                    }
                    const auto size = qFromLittleEndian<qint32>(bytes->constData() + *at - 4);
                    const auto name = textOf(rename.to.filename());
                    appended += littleEndian(qint32(name.size()));
                    for (const auto c : name) {
                        appended += littleEndian(quint16(c.unicode()));
                    }
                    appended += littleEndian(size) + bytes->mid(*at, size);
                    ++added;
                }
                const bool deleted = zeroNames(*bytes, zeroed);
                if (added == 0 && !deleted) {
                    return changes;
                }
                const auto count = qFromLittleEndian<qint32>(bytes->constData() + 8);
                qToLittleEndian(qint32(count + added), bytes->data() + 8);
                bytes->append(appended);
                changes.push_back(FileChange::rewritten("desc.mrq", *bytes));
                return changes;
            }

            // An entry is deleted as mrq_delete_entry() in mrq.c deletes it: its name is zeroed
            // and its data stays in place. Every entry of an audio file is deleted, whatever the
            // case of its name, since find() would take any of them.
            std::optional<QList<FileChange>>
                changesRemoving(const std::filesystem::path &directory,
                                const QList<std::filesystem::path> &wavs,
                                DiagnosticList &diagnostics) const override {
                const auto path = directory / "desc.mrq";
                std::error_code error;
                if (!std::filesystem::is_regular_file(path, error)) {
                    return QList<FileChange>();
                }
                auto bytes = contentsOf(path, diagnostics);
                if (!bytes) {
                    return std::nullopt;
                }
                QSet<QString> names;
                for (const auto &wav : wavs) {
                    names.insert(textOf(wav.filename()).toCaseFolded());
                }
                if (!zeroNames(*bytes, names)) {
                    return QList<FileChange>();
                }
                return QList<FileChange>{FileChange::rewritten("desc.mrq", *bytes)};
            }

        private:
            template <class T>
            static QByteArray littleEndian(T value) {
                QByteArray bytes(qsizetype(sizeof(T)), Qt::Uninitialized);
                qToLittleEndian(value, bytes.data());
                return bytes;
            }

            // Zeroes the name of each entry whose name, without case, is one of names, and
            // returns whether any name was zeroed
            static bool zeroNames(QByteArray &bytes, const QSet<QString> &names) {
                bool zeroed = false;
                for (const auto &[entry, data] : entriesOf(bytes)) {
                    if (!entry.isEmpty() && names.contains(entry.toCaseFolded())) {
                        // The name precedes the int32 size of the data.
                        const auto name = data - 4 - entry.size() * 2;
                        std::fill(bytes.begin() + name, bytes.begin() + data - 4, '\0');
                        zeroed = true;
                    }
                }
                return zeroed;
            }

            // The name and the position of the data of each entry, up to the first entry that is
            // cut short
            static QList<std::pair<QString, qsizetype>> entriesOf(const QByteArray &bytes) {
                QList<std::pair<QString, qsizetype>> found;
                if (!bytes.startsWith("mrq ")) {
                    return found;
                }
                Reader reader(bytes, 8);
                const auto entries = reader.next<qint32>();
                for (qint32 i = 0; entries && i < *entries; ++i) {
                    const auto length = reader.next<qint32>();
                    if (!length || *length < 0 || reader.left() < qsizetype(*length) * 2) {
                        break;
                    }
                    QString entry(*length, Qt::Uninitialized);
                    for (qint32 k = 0; k < *length; ++k) {
                        entry[k] = QChar(*reader.next<quint16>());
                    }
                    const auto size = reader.next<qint32>();
                    if (!size) {
                        break;
                    }
                    found.push_back({entry, reader.at()});
                    if (!reader.skip(*size)) {
                        break;
                    }
                }
                return found;
            }

            // The position of the data of the entry of wav, or none. The name is compared as
            // written first, and then without case, since the file system of Windows finds the
            // file either way.
            static std::optional<qsizetype> find(const QByteArray &bytes,
                                                 const std::filesystem::path &wav) {
                const auto name = textOf(wav.filename());
                std::optional<qsizetype> folded;
                for (const auto &[entry, data] : entriesOf(bytes)) {
                    if (entry == name) {
                        return data;
                    }
                    if (!folded && entry.compare(name, Qt::CaseInsensitive) == 0) {
                        folded = data;
                    }
                }
                return folded;
            }
        };

    }

    // frq, dio and mrq in this order of registration
    BuiltinFrequencyFormats::BuiltinFrequencyFormats(FrequencyFormatRegistry &registry) {
        m_registrations.push_back(FrequencyFormatRegistry::Add<Frq>(registry, "frq", {}));
        m_registrations.push_back(FrequencyFormatRegistry::Add<Dio>(registry, "dio", {}));
        m_registrations.push_back(FrequencyFormatRegistry::Add<Mrq>(registry, "mrq", {}));
    }

    BuiltinFrequencyFormats::~BuiltinFrequencyFormats() = default;

}
