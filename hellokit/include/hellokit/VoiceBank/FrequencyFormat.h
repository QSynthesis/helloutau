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

    /// Driver of a frequency table format. A frequency table is the analysis file that a
    /// resampler generates for an audio file. Built-in formats and plugin formats are both
    /// registered in FrequencyFormatRegistry. See docs/FrequencyTables.md.
    ///
    /// The interface is read-only. It neither writes tables nor executes resamplers.
    class HELLOKIT_VOICEBANK_EXPORT FrequencyFormat {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::FrequencyFormat)
    public:
        virtual ~FrequencyFormat();

        /// Returns a short identifier that is unique in a registry, such as \c frq.
        virtual QString id() const = 0;

        /// Returns the display name, which includes the name of the resampler.
        virtual QString name() const = 0;

        /// Returns the wildcard patterns of the file names of the resamplers that read this
        /// format, such as <tt>moresampler*.exe</tt>. The registry selects the format of the
        /// configured resampler by these patterns. Empty if the format is selected only
        /// manually.
        ///
        /// \sa FrequencyFormatRegistry::formatForResampler()
        virtual QStringList resamplerPatterns() const = 0;

        /// Returns whether the audio file \a wav has a table of this format.
        virtual bool exists(const std::filesystem::path &wav) const = 0;

        /// Reads the table of the audio file \a wav with the sample rate \a sampleRate.
        ///
        /// \return the table, or \c std::nullopt with the reason in \a diagnostics if the table
        ///         is missing or invalid
        virtual std::optional<FrequencyTable> read(const std::filesystem::path &wav, int sampleRate,
                                                   DiagnosticList &diagnostics) const = 0;
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMAT_H
