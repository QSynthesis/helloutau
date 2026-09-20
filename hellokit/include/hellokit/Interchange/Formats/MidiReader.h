#ifndef HELLOKIT_INTERCHANGE_FORMATS_MIDIREADER_H
#define HELLOKIT_INTERCHANGE_FORMATS_MIDIREADER_H

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeReader.h>

namespace hello::kit {

    /// Reads a standard MIDI file.
    ///
    /// UTAU imports MIDI too, and this does not follow it. That implementation gets several
    /// things wrong, and matching it would mean inheriting them, so the measure here is whether
    /// the project that comes out is right rather than whether it matches note for note. What
    /// that means in each case is written out in docs/Interchange.md.
    ///
    /// Two things MIDI holds cannot be written into a UST at all, and neither is a matter of
    /// preference:
    ///
    /// - **Several notes at once.** A UST is one voice. Notes that overlap are made to fit by
    ///   shortening the one already sounding, and notes that begin together lose all but the
    ///   highest. Both are reported.
    /// - **Text with no encoding.** MIDI says nothing about what its bytes mean, so the encoding
    ///   is asked for rather than guessed. See \c customStepId().
    ///
    /// Everything else comes across as it stands, the tempo and the silence before the first
    /// note included, even where the caller is about to discard them.
    class HELLOKIT_INTERCHANGE_EXPORT MidiReader : public InterchangeReader {
    public:
        MidiReader();
        ~MidiReader();

        QString id() const override;
        QString name() const override;
        QStringList suffixes() const override;

        QList<InterchangeOption> optionSchema() const override;

        /// Picking an encoding needs a view of its own, since the way to pick one is to look at
        /// the lyrics under each candidate and see which one is not gibberish.
        QString customStepId() const override;

        std::optional<InterchangeSource> inspect(const std::filesystem::path &path,
                                                 DiagnosticList &diagnostics) override;

    protected:
        std::optional<Project> convert(const std::filesystem::path &path,
                                       const InterchangeSource &source,
                                       const ImportRequest &request,
                                       DiagnosticList &diagnostics) override;
    };

}

#endif // HELLOKIT_INTERCHANGE_FORMATS_MIDIREADER_H
