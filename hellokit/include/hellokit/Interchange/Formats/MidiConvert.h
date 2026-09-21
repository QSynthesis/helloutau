#ifndef HELLOKIT_INTERCHANGE_FORMATS_MIDICONVERT_H
#define HELLOKIT_INTERCHANGE_FORMATS_MIDICONVERT_H

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

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
    ///   is asked for rather than guessed.
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

    /// Writes a standard MIDI file.
    ///
    /// **MIDI holds notes and lyrics and nothing else this project cares about.** The envelope,
    /// the vibrato, the pitch curve, the flags and every per note value UTAU renders with have
    /// nowhere to go, so a project written out this way and read back is a bare melody. That is
    /// not a defect to be fixed, it is what the format is, and it is reported every time rather
    /// than left for the user to discover.
    class HELLOKIT_INTERCHANGE_EXPORT MidiWriter : public InterchangeWriter {
    public:
        MidiWriter();
        ~MidiWriter();

        QString id() const override;
        QString name() const override;
        QStringList suffixes() const override;

        QList<InterchangeOption> optionSchema() const override;
        QString customStepId() const override;

    protected:
        bool convert(const Project &project, const std::filesystem::path &path,
                     const ExportRequest &request, DiagnosticList &diagnostics) override;
    };

}

#endif // HELLOKIT_INTERCHANGE_FORMATS_MIDICONVERT_H
