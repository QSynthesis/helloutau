#ifndef HELLOKIT_VOICEBANK_MISDECODEDNAMEQUESTION_P_H
#define HELLOKIT_VOICEBANK_MISDECODEDNAMEQUESTION_P_H

#include <QtCore/QStringList>

#include <hellokit/VoiceBank/VoiceBankConfiguration.h>

/// The parts of the question and of the conversion of misdecoded names that both share.
namespace hello::kit::misdecodedNames {

    bool isAscii(QStringView text);

    /// Returns \a name in Normalization Form C.
    QString normalized(const QString &name);

    /// Returns the names of \a entries , in Normalization Form C, whose audio file does not exist
    /// under its name among \a audio .
    QStringList missingOf(const QStringList &entries, const QStringList &audio);

    /// The pair of encodings that an inference proposes.
    struct Inference {
        /// The record with the encodings and without lists
        VoiceBankConfiguration::FileNameEncoding record;
        /// The number of \a missing entries that a restored name matches, zero if no entry is
        /// matched, in which case the pair is a preset
        int matched = 0;
    };

    /// Returns the pair under which the most names of \a missing find their audio files among
    /// \a audio restored. Among equals, the pair whose original encoding is \a charset comes
    /// first, then the pair that misdecodes in the system encoding. If no pair matches an entry,
    /// the pair is \a charset and the system encoding.
    Inference infer(const QStringList &audio, const QStringList &missing, const QString &charset);

}

#endif // HELLOKIT_VOICEBANK_MISDECODEDNAMEQUESTION_P_H
