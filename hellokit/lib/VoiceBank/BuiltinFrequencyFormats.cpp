#include "BuiltinFrequencyFormats.h"

#include <cstring>

#include <QtCore/QByteArray>
#include <QtCore/QFile>
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

        std::optional<FrequencyTable> damaged(const std::filesystem::path &path,
                                              DiagnosticList &diagnostics) {
            fail(diagnostics,
                 FrequencyFormat::tr("\"%1\" is no frequency table of its format, or it is cut "
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

            bool exists(const std::filesystem::path &wav) const override {
                std::error_code error;
                return std::filesystem::is_regular_file(pathOf(wav), error);
            }

            std::optional<FrequencyTable> read(const std::filesystem::path &wav, int sampleRate,
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
                FrequencyTable table;
                table.averageFrequency = *average;
                table.frames.reserve(size_t(*count));
                for (qint32 i = 0; i < *count; ++i) {
                    FrequencyTable::Frame frame;
                    frame.time = double(i) * *hop * 1000 / sampleRate;
                    frame.frequency = *reader.next<double>();
                    frame.amplitude = *reader.next<double>();
                    table.frames.push_back(frame);
                }
                return table;
            }

        private:
            static std::filesystem::path pathOf(const std::filesystem::path &wav) {
                auto name = wav.stem().u16string();
                auto extension = wav.extension().u16string();
                for (auto &c : extension) {
                    if (c == u'.') {
                        c = u'_';
                    }
                }
                return wav.parent_path() / (name + extension + u".frq");
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

            bool exists(const std::filesystem::path &wav) const override {
                std::error_code error;
                return std::filesystem::is_regular_file(pathOf(wav), error);
            }

            std::optional<FrequencyTable> read(const std::filesystem::path &wav, int sampleRate,
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
                FrequencyTable table;
                table.frames.reserve(size_t(*count));
                for (qint32 i = 0; i < *count; ++i) {
                    FrequencyTable::Frame frame;
                    frame.time = *reader.next<double>() * 1000;
                    frame.frequency = *reader.next<double>();
                    table.frames.push_back(frame);
                }
                return table;
            }

        private:
            static std::filesystem::path pathOf(const std::filesystem::path &wav) {
                auto path = wav;
                return path.replace_extension(".dio");
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

            bool exists(const std::filesystem::path &wav) const override {
                DiagnosticList ignored;
                const auto bytes = contentsOf(wav.parent_path() / "desc.mrq", ignored);
                return bytes && find(*bytes, wav).has_value();
            }

            std::optional<FrequencyTable> read(const std::filesystem::path &wav, int sampleRate,
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
                FrequencyTable table;
                table.frames.reserve(size_t(*count));
                for (qint32 i = 0; i < *count; ++i) {
                    FrequencyTable::Frame frame;
                    frame.time = double(i) * *hop * 1000 / *rate;
                    frame.frequency = *reader.next<float>();
                    table.frames.push_back(frame);
                }
                return table;
            }

        private:
            // The position of the data of the entry of wav, or none. The name is compared as
            // written first, and then without case, since the file system of Windows finds the
            // file either way.
            static std::optional<qsizetype> find(const QByteArray &bytes,
                                                 const std::filesystem::path &wav) {
                if (!bytes.startsWith("mrq ")) {
                    return std::nullopt;
                }
                const auto name = textOf(wav.filename());
                Reader reader(bytes, 8);
                const auto entries = reader.next<qint32>();
                if (!entries) {
                    return std::nullopt;
                }
                std::optional<qsizetype> folded;
                for (qint32 i = 0; i < *entries; ++i) {
                    const auto length = reader.next<qint32>();
                    if (!length || *length < 0 || reader.left() < qsizetype(*length) * 2) {
                        return folded;
                    }
                    QString entry(*length, Qt::Uninitialized);
                    for (qint32 k = 0; k < *length; ++k) {
                        entry[k] = QChar(*reader.next<quint16>());
                    }
                    const auto size = reader.next<qint32>();
                    if (!size) {
                        return folded;
                    }
                    const auto data = reader.at();
                    if (entry == name) {
                        return data;
                    }
                    if (!folded && entry.compare(name, Qt::CaseInsensitive) == 0) {
                        folded = data;
                    }
                    if (!reader.skip(*size)) {
                        return folded;
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
