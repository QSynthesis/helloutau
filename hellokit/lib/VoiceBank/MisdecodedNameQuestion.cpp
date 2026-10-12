#include "MisdecodedNameQuestion.h"

#include <algorithm>

#include <QtCore/QSet>

#include <hellokit/Support/TextCodec.h>

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        bool isAscii(QStringView text) {
            return std::all_of(text.begin(), text.end(),
                               [](QChar c) { return c.unicode() < 0x80; });
        }

        QString normalized(const QString &name) {
            return name.normalized(QString::NormalizationForm_C);
        }

        QString textOf(const fs::path &name) {
            return QString::fromStdU16String(name.u16string());
        }

        /// The names of the audio files of \a source , in the order of the listing.
        QStringList audioNamesOf(const VoiceBankDirectorySource &source) {
            QStringList names;
            for (const auto &name : source.audioFiles) {
                names.push_back(textOf(name));
            }
            return names;
        }

        /// The names of the subdirectories of \a source , in the order of its stamp.
        QStringList directoryNamesOf(const VoiceBankDirectorySource &source) {
            QStringList names;
            for (const auto &entry : source.stamp.entries) {
                if (entry.directory) {
                    names.push_back(textOf(entry.name));
                }
            }
            return names;
        }

        /// The file names of the entries of the oto.ini of \a source decoded in \a charset , in
        /// Normalization Form C.
        QStringList entryNamesOf(const VoiceBankDirectorySource &source, const QString &charset) {
            QStringList names;
            if (!source.oto) {
                return names;
            }
            const TextCodec codec(charset);
            for (const auto &[file, entries] : source.oto->contents) {
                Q_UNUSED(entries)
                names.push_back(normalized(
                    codec.decodeReplacing(QByteArrayView(file.data(), qsizetype(file.size())))));
            }
            return names;
        }

        /// The names of \a entries whose audio file does not exist under its name on disk.
        QStringList missingOf(const QStringList &entries, const QStringList &audio) {
            QSet<QString> present;
            for (const auto &name : audio) {
                present.insert(normalized(name));
            }
            QStringList missing;
            for (const auto &name : entries) {
                if (!present.contains(name)) {
                    missing.push_back(name);
                }
            }
            return missing;
        }

        /// The number of \a missing that a name of \a audio restores to under \a record .
        int matchesOf(const VoiceBankConfig::FileNameEncoding &record, const QStringList &audio,
                      const QStringList &missing) {
            QSet<QString> restored;
            for (const auto &name : audio) {
                if (const auto logical = record.restore(name)) {
                    restored.insert(*logical);
                }
            }
            return int(
                std::count_if(missing.begin(), missing.end(), [&restored](const QString &name) {
                    return restored.contains(name);
                }));
        }

    }

    std::optional<MisdecodedNameQuestion>
        MisdecodedNameQuestion::of(const VoiceBankDirectorySource &source, const QString &charset) {
        const auto entries = entryNamesOf(source, charset);
        const auto audio = audioNamesOf(source);
        const auto missing = missingOf(entries, audio);

        MisdecodedNameQuestion question;
        if (source.config && source.config->fileNameEncoding) {
            question.m_record = *source.config->fileNameEncoding;
            question.m_recorded = true;
        } else {
            if (missing.isEmpty()) {
                return std::nullopt;
            }
            // The pair under which the most missing entries find their audio files. Among
            // equals, the pair that misdecodes in the system encoding comes first, because the
            // files were most likely extracted on this machine, then the order of the candidates.
            const auto candidates = TextCodec::ustCandidates();
            const auto system = TextCodec(TextCodec::systemName()).name();
            std::optional<VoiceBankConfig::FileNameEncoding> best;
            int bestMatched = 0;
            for (const auto &original : candidates) {
                for (const auto &misdecodedAs : candidates) {
                    VoiceBankConfig::FileNameEncoding record;
                    record.original = TextCodec(original).name();
                    record.misdecodedAs = TextCodec(misdecodedAs).name();
                    if (record.original == record.misdecodedAs) {
                        continue;
                    }
                    const int matched = matchesOf(record, audio, missing);
                    const bool better = matched > bestMatched || (best && matched == bestMatched &&
                                                                  record.misdecodedAs == system &&
                                                                  best->misdecodedAs != system);
                    if (better) {
                        best = record;
                        bestMatched = matched;
                    }
                }
            }
            if (!best) {
                return std::nullopt;
            }
            question.m_record = *best;
        }
        question.collect(source, entries, missing);
        if (question.m_choices.isEmpty()) {
            return std::nullopt;
        }
        return question;
    }

    MisdecodedNameQuestion MisdecodedNameQuestion::of(const VoiceBankDirectorySource &source,
                                                      const QString &charset,
                                                      const QString &original,
                                                      const QString &misdecodedAs) {
        MisdecodedNameQuestion question;
        question.m_record.original = TextCodec(original).name();
        question.m_record.misdecodedAs = TextCodec(misdecodedAs).name();
        const auto entries = entryNamesOf(source, charset);
        question.collect(source, entries, missingOf(entries, audioNamesOf(source)));
        return question;
    }

    void MisdecodedNameQuestion::collect(const VoiceBankDirectorySource &source,
                                         const QStringList &entries, const QStringList &missing) {
        m_directory = source.path;

        // The question about a directory with a record covers the names in neither list only.
        QSet<QString> listed;
        if (m_recorded) {
            for (const auto &name : m_record.misdecoded + m_record.correct) {
                listed.insert(normalized(name));
            }
        }
        QSet<QString> named;
        for (const auto &name : entries) {
            named.insert(name);
        }
        QSet<QString> restored;
        const auto add = [&](const QString &name, bool directory) {
            if (isAscii(name) || listed.contains(normalized(name))) {
                return;
            }
            const auto logical = m_record.restore(name);
            if (!logical) {
                m_unrestorable.push_back(name);
                return;
            }
            // A name on disk that an entry names is in use as it is.
            m_choices.push_back({name, *logical, !named.contains(normalized(name)), directory});
            restored.insert(*logical);
        };
        for (const auto &name : audioNamesOf(source)) {
            add(name, false);
        }
        for (const auto &name : directoryNamesOf(source)) {
            add(name, true);
        }

        m_missing = int(missing.size());
        m_matched =
            int(std::count_if(missing.begin(), missing.end(), [&restored](const QString &name) {
                return restored.contains(name);
            }));
    }

    VoiceBankConfig::FileNameEncoding
        MisdecodedNameQuestion::answer(const QStringList &misdecoded) const {
        auto record = m_record;
        for (const auto &choice : m_choices) {
            (misdecoded.contains(choice.name) ? record.misdecoded : record.correct)
                .push_back(choice.name);
        }
        record.correct += m_unrestorable;
        return record;
    }

}
