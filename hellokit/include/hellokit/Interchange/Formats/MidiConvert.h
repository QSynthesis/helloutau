#ifndef HELLOKIT_INTERCHANGE_FORMATS_MIDICONVERT_H
#define HELLOKIT_INTERCHANGE_FORMATS_MIDICONVERT_H

#include <QtCore/QCoreApplication>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    /// Reads a standard MIDI file.
    ///
    /// UTAU also imports MIDI, and this implementation deliberately deviates from it. The UTAU
    /// importer has several defects, and matching it would reproduce them. The criterion here
    /// is therefore the correctness of the resulting project, not note-for-note agreement with
    /// UTAU. docs/Interchange.md specifies the behavior in each case.
    ///
    /// Two properties of MIDI cannot be represented in a UST, and neither is a matter of
    /// preference:
    ///
    /// - **Simultaneous notes.** A UST is monophonic. Overlapping notes are resolved by
    ///   shortening the note already sounding, and of notes that start together only the
    ///   highest is kept. Both cases are reported.
    /// - **Text without a declared encoding.** MIDI does not specify the encoding of its text,
    ///   so the encoding is requested from the user rather than guessed.
    ///
    /// All other content is converted unchanged, including the tempo and the silence before the
    /// first note, even if the caller discards them afterward.
    class HELLOKIT_INTERCHANGE_EXPORT MidiReader : public InterchangeReader {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::MidiReader)
    public:
        MidiReader();
        ~MidiReader();

        QString id() const override;
        QString name() const override;
        QStringList suffixes() const override;

        QList<InterchangeOption> optionSchema() const override;

        /// Encoding selection requires a custom view, because the correct encoding is identified
        /// by comparing the lyrics as decoded under each candidate.
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
    /// **MIDI represents notes and lyrics and no other data relevant to this project.** The
    /// envelope, the vibrato, the pitch curve, the flags and every per-note rendering parameter
    /// of UTAU cannot be represented, so a project exported this way and imported again is a
    /// bare melody. This is a limitation of the format, not a defect, and it is reported on
    /// every export rather than left for the user to discover.
    class HELLOKIT_INTERCHANGE_EXPORT MidiWriter : public InterchangeWriter {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::MidiWriter)
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
