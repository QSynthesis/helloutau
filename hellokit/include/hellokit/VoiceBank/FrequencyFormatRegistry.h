#ifndef HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRY_H
#define HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRY_H

#include <filesystem>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/VoiceBank/FrequencyFormat.h>
#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// The formats of frequency tables that this build reads. See docs/FrequencyTables.md.
    ///
    /// Built-in formats and those of plugins are registered alike, as in InterchangeRegistry,
    /// and like it this is no singleton: the application owns one and passes it on.
    class HELLOKIT_VOICEBANK_EXPORT FrequencyFormatRegistry {
    public:
        FrequencyFormatRegistry();
        ~FrequencyFormatRegistry();

        /// Adds the formats of this library: frq of resampler.exe, dio of world4utau and mrq of
        /// moresampler, the formats whose layouts are public.
        void addBuiltinFormats();

        /// Takes ownership. A format whose ID is already registered is rejected. Returns whether
        /// the format was added.
        bool add(std::unique_ptr<FrequencyFormat> format);

        /// In the order of registration.
        QList<FrequencyFormat *> formats() const;

        FrequencyFormat *format(const QString &id) const;

        /// Returns the format that the resampler \a resampler reads: the first registered whose
        /// patterns match its file name, compared case-insensitively, or else \c frq, which the
        /// resampler of UTAU reads; null if neither is registered.
        ///
        /// Only the file name is looked at. Nothing is run.
        FrequencyFormat *formatForResampler(const std::filesystem::path &resampler) const;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY_MOVE(FrequencyFormatRegistry)
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRY_H
