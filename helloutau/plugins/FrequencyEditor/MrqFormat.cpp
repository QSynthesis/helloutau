#include "MrqFormat.h"

#include <algorithm>

#include <QtCore/QDateTime>

#include <hellokit/Support/BinaryStream.h>

namespace hello::daw {

    // The layout, as the mrq library of the author of moresampler reads it
    // (https://github.com/Sleepwalking/mrq/blob/6b2df0d5965bc4dce8c99be6a3eb1325ba94718e/mrq.h and
    // mrq.c): "mrq ", the int32 version and the int32 entry count; each entry the int32 length of
    // its name, the name in UTF-16, the int32 size of its data, then the int32 frame count, the
    // int32 sample rate, the int32 hop in samples, a float frequency per frame, and in version 2 a
    // timestamp and a flag. A deleted entry keeps its place with its name zeroed. moresampler
    // names an entry after the audio file in UTF-16, measured with moresampler 0.8.4 on
    // 2026-10-12 (docs/FrequencyMaps.md).

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

    }

    QString MrqFormat::id() const {
        return QLatin1String(formatId);
    }

    QString MrqFormat::name() const {
        return tr("mrq (moresampler)");
    }

    QStringList MrqFormat::resamplerPatterns() const {
        return {QStringLiteral("moresampler*.exe")};
    }

    QList<bool> MrqFormat::mapsExist(const std::filesystem::path &directory,
                                     const QList<std::filesystem::path> &wavs) const {
        kit::DiagnosticList ignored;
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

    std::optional<kit::FrequencyMap> MrqFormat::read(const std::filesystem::path &wav,
                                                     int sampleRate,
                                                     kit::DiagnosticList &diagnostics) const {
        Q_UNUSED(sampleRate);
        const auto path = wav.parent_path() / "desc.mrq";
        const auto bytes = contentsOf(path, diagnostics);
        if (!bytes) {
            return std::nullopt;
        }
        const auto at = find(*bytes, wav);
        if (!at) {
            diagnostics.push_back(
                {kit::DiagnosticSeverity::Error,
                 tr("\"%1\" has no entry for \"%2\".").arg(textOf(path), textOf(wav.filename()))});
            return std::nullopt;
        }
        kit::BinaryStream stream(*bytes, at->second);
        const auto count = stream.read<qint32>();
        const auto rate = stream.read<qint32>();
        const auto hop = stream.read<qint32>();
        if (!count || !rate || !hop || *count < 0 || *rate <= 0 || *hop <= 0 ||
            stream.bytesAvailable() < qsizetype(*count) * 4) {
            return damaged(path, diagnostics);
        }
        kit::FrequencyMap table;
        table.hop = *hop;
        table.frames.reserve(size_t(*count));
        for (qint32 i = 0; i < *count; ++i) {
            kit::FrequencyMap::Frame frame;
            frame.time = double(i) * *hop * 1000 / *rate;
            frame.frequency = *stream.read<float>();
            table.frames.push_back(frame);
        }
        return table;
    }

    std::optional<std::filesystem::path>
        MrqFormat::mapFileNameOf(const std::filesystem::path &wav) const {
        Q_UNUSED(wav);
        return std::filesystem::path("desc.mrq");
    }

    // moresampler derives the LLSM file from the entry and writes it again if missing, measured
    // as above. Its readme states that desc.mrq is read before an LLSM file is created, so that a
    // deleted entry is written again only if the LLSM file is missing as well.
    QList<kit::FrequencyFormat::AnalysisCache> MrqFormat::analysisCaches() const {
        return {
            {QStringLiteral("LLSM"), AnalysisCache::AppendedSuffix, QStringLiteral(".llsm")},
        };
    }

    // As mrq_write_entry() in mrq.c writes an entry that does not fit: the old name is zeroed and
    // the entry is appended.
    std::optional<QList<kit::FrequencyFormat::FileChange>>
        MrqFormat::changesRenaming(const std::filesystem::path &directory,
                                   const QList<AudioRename> &renames,
                                   kit::DiagnosticList &diagnostics) const {
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
        // The data of every renamed entry is taken before any name is zeroed, so that two audio
        // files may exchange their names.
        QByteArray appended;
        kit::BinaryStream append(&appended);
        qint32 added = 0;
        QSet<QString> zeroed;
        for (const auto &rename : renames) {
            const auto at = find(*bytes, rename.from);
            zeroed.insert(textOf(rename.from.filename()).toCaseFolded());
            zeroed.insert(textOf(rename.to.filename()).toCaseFolded());
            if (!at) {
                continue;
            }
            // The int32 size precedes the data. An entry cut short is not written again.
            kit::BinaryStream entry(*bytes, at->second - 4);
            const auto size = entry.read<qint32>();
            const auto data = size ? entry.readBytes(*size) : std::nullopt;
            if (!data) {
                continue;
            }
            const auto name = textOf(rename.to.filename());
            append.write(qint32(name.size()));
            for (const auto c : name) {
                append.write(quint16(c.unicode()));
            }
            append.write(*size);
            append.writeBytes(*data);
            ++added;
        }
        const bool deleted = zeroNames(*bytes, zeroed);
        if (added == 0 && !deleted) {
            return changes;
        }
        kit::BinaryStream header(&*bytes, 8);
        const auto count = header.read<qint32>().value_or(0);
        header.seek(8);
        header.write(qint32(count + added));
        bytes->append(appended);
        changes.push_back(FileChange::rewritten("desc.mrq", *bytes));
        return changes;
    }

    // As mrq_delete_entry() in mrq.c deletes an entry: its name is zeroed and its data stays in
    // place. Every entry of an audio file is deleted, since find() would take any of them.
    std::optional<QList<kit::FrequencyFormat::FileChange>>
        MrqFormat::changesRemoving(const std::filesystem::path &directory,
                                   const QList<std::filesystem::path> &wavs,
                                   kit::DiagnosticList &diagnostics) const {
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

    bool MrqFormat::canWrite() const {
        return true;
    }

    // As mrq_write_entry() in mrq.c writes an entry: an entry with room for the frames is
    // overwritten in place and keeps its size, and another entry has its name zeroed and is
    // appended. The version in the header becomes 2, the timestamp is the time of writing, and
    // the flag is 0 for an entry edited by hand and 1 otherwise. An entry of version 1 has no
    // room for the timestamp and is appended. The sample rate of an existing entry is kept.
    std::optional<QList<kit::FrequencyFormat::FileChange>>
        MrqFormat::changesWriting(const std::filesystem::path &directory,
                                  const QList<MapWrite> &writes,
                                  kit::DiagnosticList &diagnostics) const {
        const auto path = directory / "desc.mrq";
        QByteArray bytes("mrq ");
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error)) {
            const auto existing = contentsOf(path, diagnostics);
            if (!existing) {
                return std::nullopt;
            }
            if (!existing->startsWith("mrq ") || existing->size() < 12) {
                damaged(path, diagnostics);
                return std::nullopt;
            }
            bytes = *existing;
        } else {
            kit::BinaryStream header(&bytes, bytes.size());
            header.write(qint32(2));
            header.write(qint32(0));
        }
        const auto timestamp = qint32(QDateTime::currentSecsSinceEpoch());
        for (const auto &write : writes) {
            const auto hop = integralHop(write, diagnostics);
            if (!hop) {
                return std::nullopt;
            }
            const auto &frames = write.map.frames;
            const auto needed = 20 + qsizetype(frames.size()) * 4;
            std::optional<qsizetype> at;
            std::optional<qint32> rate;
            if (const auto entry = find(bytes, write.wav)) {
                const auto &[name, data] = *entry;
                kit::BinaryStream existing(bytes, data - 4);
                const auto size = existing.read<qint32>();
                existing.skip(4);
                rate = existing.read<qint32>();
                if (size && *size >= needed) {
                    at = data;
                } else {
                    // The name precedes the int32 size of the data.
                    std::fill(bytes.begin() + data - 4 - name.size() * 2, bytes.begin() + data - 4,
                              '\0');
                }
            }
            if (!rate || *rate <= 0) {
                rate = write.sampleRate;
            }
            if (*rate <= 0) {
                return unwritable(write.wav, tr("The sample rate of the audio file is unknown."),
                                  diagnostics);
            }
            if (!at) {
                kit::BinaryStream header(&bytes, 8);
                const auto count = header.read<qint32>().value_or(0);
                header.seek(8);
                header.write(qint32(count + 1));
                const auto name = textOf(write.wav.filename());
                kit::BinaryStream entry(&bytes, bytes.size());
                entry.write(qint32(name.size()));
                for (const auto c : name) {
                    entry.write(quint16(c.unicode()));
                }
                entry.write(qint32(needed));
                at = entry.pos();
            }
            kit::BinaryStream data(&bytes, *at);
            data.write(qint32(frames.size()));
            data.write(*rate);
            data.write(*hop);
            for (const auto &frame : frames) {
                data.write(float(frame.frequency));
            }
            data.write(timestamp);
            data.write(qint32(write.editedByHand ? 0 : 1));
        }
        kit::BinaryStream header(&bytes, 4);
        header.write(qint32(2));
        return QList<FileChange>{FileChange::rewritten("desc.mrq", bytes)};
    }

    bool MrqFormat::zeroNames(QByteArray &bytes, const QSet<QString> &names) {
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

    QList<std::pair<QString, qsizetype>> MrqFormat::entriesOf(const QByteArray &bytes) {
        QList<std::pair<QString, qsizetype>> found;
        if (!bytes.startsWith("mrq ")) {
            return found;
        }
        kit::BinaryStream stream(bytes, 8);
        const auto entries = stream.read<qint32>();
        for (qint32 i = 0; entries && i < *entries; ++i) {
            const auto length = stream.read<qint32>();
            if (!length || *length < 0 || stream.bytesAvailable() < qsizetype(*length) * 2) {
                break;
            }
            QString entry(*length, Qt::Uninitialized);
            for (qint32 k = 0; k < *length; ++k) {
                entry[k] = QChar(*stream.read<quint16>());
            }
            const auto size = stream.read<qint32>();
            if (!size) {
                break;
            }
            found.push_back({entry, stream.pos()});
            if (!stream.skip(*size)) {
                break;
            }
        }
        return found;
    }

    // The name is compared as written first, and then without case, since the file system of
    // Windows finds the file either way.
    std::optional<std::pair<QString, qsizetype>> MrqFormat::find(const QByteArray &bytes,
                                                                 const std::filesystem::path &wav) {
        const auto name = textOf(wav.filename());
        std::optional<std::pair<QString, qsizetype>> folded;
        for (const auto &entry : entriesOf(bytes)) {
            if (entry.first == name) {
                return entry;
            }
            if (!folded && entry.first.compare(name, Qt::CaseInsensitive) == 0) {
                folded = entry;
            }
        }
        return folded;
    }

}
