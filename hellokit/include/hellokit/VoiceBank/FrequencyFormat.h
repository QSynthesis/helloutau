#ifndef HELLOKIT_VOICEBANK_FREQUENCYFORMAT_H
#define HELLOKIT_VOICEBANK_FREQUENCYFORMAT_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/VoiceBank/FrequencyTable.h>
#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// The driver of one format of frequency tables, the file that a resampler analyzes an
    /// audio file into. Built-in formats and those of plugins are registered alike in a
    /// FrequencyFormatRegistry. See docs/FrequencyTables.md.
    ///
    /// A driver only reads: nothing here writes a table or runs a resampler.
    class HELLOKIT_VOICEBANK_EXPORT FrequencyFormat {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::FrequencyFormat)
    public:
        virtual ~FrequencyFormat();

        /// A short identifier, unique in a registry, such as \c frq.
        virtual QString id() const = 0;

        /// The name shown to the user, which names the resampler as well.
        virtual QString name() const = 0;

        /// The wildcard patterns of the file names of the resamplers that read the format, such
        /// as <tt>moresampler*.exe</tt>, by which the format of the resampler in use is chosen.
        /// Empty if the format is chosen only by hand.
        ///
        /// \sa FrequencyFormatRegistry::formatForResampler()
        virtual QStringList resamplerPatterns() const = 0;

        /// Returns whether the audio file \a wav has a table of this format.
        virtual bool exists(const std::filesystem::path &wav) const = 0;

        /// Reads the table of the audio file \a wav, whose sample rate is \a sampleRate.
        ///
        /// \return the table, or \c std::nullopt with the reason in \a diagnostics if it is
        ///         missing or does not read
        virtual std::optional<FrequencyTable> read(const std::filesystem::path &wav, int sampleRate,
                                                   DiagnosticList &diagnostics) const = 0;
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMAT_H
