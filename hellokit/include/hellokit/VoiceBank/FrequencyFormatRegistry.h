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

    /// The formats of frequency tables registered in the process, which come and go with their
    /// registrations. See docs/FrequencyTables.md.
    ///
    /// Built-in formats and those of plugins are registered alike by
    /// FrequencyFormatRegistration. The registry is no singleton: the application owns one and
    /// passes it on, and each registry signals the changes.
    class HELLOKIT_VOICEBANK_EXPORT FrequencyFormatRegistry : public QObject {
        Q_OBJECT
    public:
        explicit FrequencyFormatRegistry(QObject *parent = nullptr);
        ~FrequencyFormatRegistry();

        /// In the order of registration. Of formats of the same ID, only the first registered is
        /// included. A format goes when its registration goes.
        QList<FrequencyFormat *> formats() const;

        FrequencyFormat *format(const QString &id) const;

        /// Returns the format that the resampler \a resampler reads: the last registered whose
        /// patterns match its file name, compared case-insensitively, or else \c frq, which the
        /// resampler of UTAU reads; null if neither is registered. A plugin registered later
        /// thereby takes over a resampler from a format before it.
        ///
        /// Only the file name is looked at. Nothing is run.
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
