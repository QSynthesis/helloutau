#ifndef HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRY_H
#define HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRY_H

#include <filesystem>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <hellokit/VoiceBank/FrequencyFormat.h>
#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// The frequency table formats registered in the process. The content changes as
    /// registrations are created and destroyed. See docs/FrequencyTables.md.
    ///
    /// Built-in formats and plugin formats are both registered by FrequencyFormatRegistration.
    /// The registry is not a singleton. The owner passes it to its users, and each registry
    /// emits formatsChanged().
    class HELLOKIT_VOICEBANK_EXPORT FrequencyFormatRegistry : public QObject {
        Q_OBJECT
    public:
        explicit FrequencyFormatRegistry(QObject *parent = nullptr);
        ~FrequencyFormatRegistry();

        /// Returns the formats in the order of registration. Of several formats with the same
        /// ID, only the first registered is included. A format is removed when its registration
        /// is destroyed.
        QList<FrequencyFormat *> formats() const;

        FrequencyFormat *format(const QString &id) const;

        /// Returns the format read by the resampler \a resampler: the last registered format
        /// whose patterns match the file name case-insensitively. A format registered later
        /// therefore takes precedence over an earlier format for the same resampler. Only the
        /// file name is examined. The resampler is not executed.
        ///
        /// \return the matching format, else \c frq (the format of the UTAU resampler), or null
        ///         if neither is registered
        FrequencyFormat *formatForResampler(const std::filesystem::path &resampler) const;

    Q_SIGNALS:
        /// Emitted after a format was registered or unregistered.
        void formatsChanged();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY_MOVE(FrequencyFormatRegistry)
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRY_H
