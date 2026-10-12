#include "MisdecodedNameConversion.h"

#include <algorithm>

#include <QtCore/QSet>

#include <hellokit/Support/TextCodec.h>

#include "MisdecodedNameQuestion_p.h"

namespace hello::kit {

    using namespace misdecodedNames;

    namespace {

        bool lists(const QStringList &list, const QString &name) {
            return std::any_of(list.begin(), list.end(), [&name](const QString &item) {
                return normalized(item) == normalized(name);
            });
        }

    }

    MisdecodedNameConversion MisdecodedNameConversion::of(const Listing &listing) {
        if (listing.record && !listing.record->isAsIs()) {
            return of(listing, listing.record->original, listing.record->misdecodedAs);
        }
        const auto inference = infer(
            listing.audioFiles, missingOf(listing.entries, listing.audioFiles), listing.charset);
        if (inference.matched > 0) {
            return of(listing, inference.record.original, inference.record.misdecodedAs);
        }

        // Misdecoded in the system encoding, from the candidate in which the bytes of the names
        // read best.
        const TextCodec system(TextCodec::systemName());
        QList<QByteArray> bytes;
        for (const auto &name : listing.audioFiles + listing.subdirectories) {
            if (!isAscii(name) && system.canEncode(name)) {
                bytes.push_back(system.encode(name));
            }
        }
        QStringList candidates;
        for (const auto &candidate : TextCodec::ustCandidates()) {
            if (TextCodec(candidate).name() != system.name()) {
                candidates.push_back(candidate);
            }
        }
        return of(listing, TextCodec::ranked(bytes, candidates).value(0), system.name());
    }

    MisdecodedNameConversion MisdecodedNameConversion::of(const Listing &listing,
                                                          const QString &original,
                                                          const QString &misdecodedAs) {
        MisdecodedNameConversion conversion;
        conversion.m_directory = listing.directory;
        conversion.m_original = TextCodec(original).name();
        conversion.m_misdecodedAs = TextCodec(misdecodedAs).name();
        conversion.m_record = listing.record;
        conversion.collect(listing);
        return conversion;
    }

    void MisdecodedNameConversion::collect(const Listing &listing) {
        VoiceBankConfiguration::FileNameEncoding encodings;
        encodings.original = m_original;
        encodings.misdecodedAs = m_misdecodedAs;
        QSet<QString> named;
        for (const auto &name : listing.entries) {
            named.insert(normalized(name));
        }
        const auto add = [&](const QString &name, bool directory) {
            if (isAscii(name)) {
                return;
            }
            const auto restored = encodings.restore(name);
            if (!restored) {
                m_unrestorable.push_back(name);
                return;
            }
            // A name on disk that an entry names is in use as it is.
            bool misdecoded = !named.contains(normalized(name));
            if (m_record) {
                if (m_record->isAsIs()) {
                    misdecoded = false;
                } else if (!m_record->files) {
                    misdecoded = true;
                } else if (lists(m_record->files->misdecoded, name)) {
                    misdecoded = true;
                } else if (lists(m_record->files->correct, name)) {
                    misdecoded = false;
                }
            }
            m_choices.push_back({name, *restored, misdecoded, directory});
        };
        for (const auto &name : listing.audioFiles) {
            add(name, false);
        }
        for (const auto &name : listing.subdirectories) {
            add(name, true);
        }
    }

    VoiceBankConfiguration::FileNameEncoding
        MisdecodedNameConversion::record(const QStringList &misdecoded) const {
        QStringList checked;
        QStringList correct;
        for (const auto &choice : m_choices) {
            (misdecoded.contains(choice.name) ? checked : correct).push_back(choice.name);
        }
        correct += m_unrestorable;
        return VoiceBankConfiguration::FileNameEncoding::normalized(m_original, m_misdecodedAs,
                                                                    checked, correct);
    }

    VoiceBankConfiguration::FileNameEncoding MisdecodedNameConversion::keptRecord() const {
        VoiceBankConfiguration::FileNameEncoding record;
        if (m_record && !m_record->isAsIs()) {
            record = *m_record;
        } else {
            record.files = VoiceBankConfiguration::FileNameEncoding::Files();
        }
        record.original = m_original;
        record.misdecodedAs = m_misdecodedAs;
        return record;
    }

}
