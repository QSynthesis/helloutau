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
        int matchesOf(const VoiceBankConfiguration::FileNameEncoding &record,
                      const QStringList &audio, const QStringList &missing) {
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
        const auto text = TextCodec(charset);
        const auto system = TextCodec(TextCodec::systemName()).name();
        const auto candidates = TextCodec::ustCandidates();

        MisdecodedNameQuestion question;
        if (source.config && source.config->fileNameEncoding) {
            if (!source.config->fileNameEncoding->files) {
                return std::nullopt;
            }
            question.m_record = *source.config->fileNameEncoding;
            question.m_basis = Record;
        } else {
            // The pair under which the most missing entries find their audio files. Among
            // equals, the pair that was written in the encoding of the text files comes first,
            // then the pair that misdecodes in the system encoding, because the files were most
            // likely extracted on this machine, then the order of the candidates.
            const auto rank = [&](const VoiceBankConfiguration::FileNameEncoding &record) {
                return int(record.original != text.name()) * 2 + int(record.misdecodedAs != system);
            };
            std::optional<VoiceBankConfiguration::FileNameEncoding> best;
            int bestMatched = 0;
            for (const auto &original : candidates) {
                for (const auto &misdecodedAs : candidates) {
                    VoiceBankConfiguration::FileNameEncoding record;
                    record.original = TextCodec(original).name();
                    record.misdecodedAs = TextCodec(misdecodedAs).name();
                    if (record.original == record.misdecodedAs) {
                        continue;
                    }
                    const int matched = missing.isEmpty() ? 0 : matchesOf(record, audio, missing);
                    if (matched > bestMatched ||
                        (best && matched == bestMatched && rank(record) < rank(*best))) {
                        best = record;
                        bestMatched = matched;
                    }
                }
            }
            if (best) {
                question.m_record = *best;
                question.m_basis = Entries;
            } else {
                // The encoding of the text files, misdecoded in the system encoding
                auto &record = question.m_record;
                record.original = text.isValid() && !charset.isEmpty()
                                      ? text.name()
                                      : TextCodec(candidates.value(0)).name();
                record.misdecodedAs = system;
                for (qsizetype i = 0;
                     record.misdecodedAs == record.original && i < candidates.size(); ++i) {
                    record.misdecodedAs = TextCodec(candidates.at(i)).name();
                }
            }
        }
        question.collect(source, entries, missing);
        if (question.m_choices.isEmpty() || (!question.isRecorded() && question.m_unnamed == 0)) {
            return std::nullopt;
        }

        // Most directories have no misdecoded name: the entries find their files, and the names
        // are names of the encoding of the text files.
        if (question.m_basis == None) {
            auto names = question.m_unrestorable;
            for (const auto &choice : std::as_const(question.m_choices)) {
                names.push_back(choice.name);
            }
            const bool found = !entries.isEmpty() && question.m_found * 4 >= entries.size() * 3;
            const bool representable =
                text.isValid() && !charset.isEmpty() &&
                std::all_of(names.begin(), names.end(),
                            [&text](const QString &name) { return text.canEncode(name); });
            if (found && representable) {
                question.m_basis = Found;
                for (auto &choice : question.m_choices) {
                    choice.misdecoded = false;
                }
            }
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
        question.m_basis = question.m_matched > 0 ? Entries : None;
        return question;
    }

    void MisdecodedNameQuestion::collect(const VoiceBankDirectorySource &source,
                                         const QStringList &entries, const QStringList &missing) {
        m_directory = source.path;

        // The question about a directory with a record covers the names in neither list only.
        QSet<QString> listed;
        if (m_record.files) {
            for (const auto &name : m_record.files->misdecoded + m_record.files->correct) {
                listed.insert(normalized(name));
            }
        }
        QSet<QString> named;
        for (const auto &name : entries) {
            named.insert(name);
        }
        QSet<QString> missingNames;
        for (const auto &name : missing) {
            missingNames.insert(name);
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
            const bool isNamed = named.contains(normalized(name));
            m_choices.push_back({name, *logical, !isNamed, directory});
            restored.insert(*logical);
            m_unnamed += !isNamed;
            m_unlisted += !isNamed && !missingNames.contains(*logical);
        };
        for (const auto &name : audioNamesOf(source)) {
            add(name, false);
        }
        for (const auto &name : directoryNamesOf(source)) {
            add(name, true);
        }

        m_missing = int(missing.size());
        m_found = int(entries.size()) - m_missing;
        m_matched =
            int(std::count_if(missing.begin(), missing.end(), [&restored](const QString &name) {
                return restored.contains(name);
            }));
    }

    VoiceBankConfiguration::FileNameEncoding
        MisdecodedNameQuestion::answer(const QStringList &misdecoded) const {
        auto record = m_record;
        auto files = record.files.value_or(VoiceBankConfiguration::FileNameEncoding::Files());
        for (const auto &choice : m_choices) {
            (misdecoded.contains(choice.name) ? files.misdecoded : files.correct)
                .push_back(choice.name);
        }
        files.correct += m_unrestorable;
        return VoiceBankConfiguration::FileNameEncoding::normalized(
            record.original, record.misdecodedAs, files.misdecoded, files.correct);
    }
}
