#ifndef HELLOUTAU_INTERCHANGE_MIDIENCODINGPAGE_H
#define HELLOUTAU_INTERCHANGE_MIDIENCODINGPAGE_H

#include <QtCore/QPointer>

#include <Interchange/InterchangeStepPage.h>

class QListWidget;
class QPlainTextEdit;
class QVBoxLayout;

namespace hello::daw {

    class InterchangeOptionForm;

    /// Custom step \c midi.encoding of the MIDI import driver: the encoding list, a preview of
    /// the track names, lyrics and markers decoded with the selected encoding, and a form for
    /// the other options of the driver. See the encoding page in docs/ImportExport.md.
    ///
    /// Encodings that cannot decode the text of the file are shown in the disabled text color.
    class MidiEncodingPage : public InterchangeStepPage {
        Q_OBJECT
    public:
        /// The ID that MidiReader::customStepId() returns
        static constexpr char stepId[] = "midi.encoding";

        explicit MidiEncodingPage(QWidget *parent = nullptr);
        ~MidiEncodingPage() override;

        void reset(const kit::InterchangeReader &reader,
                   const kit::InterchangeSource &source) override;
        bool apply(kit::ImportRequest &request) const override;
        bool isComplete() const override;

    private:
        QListWidget *m_encodings;
        QPlainTextEdit *m_preview;
        QVBoxLayout *m_layout;
        QPointer<InterchangeOptionForm> m_form;
        kit::InterchangeSource m_source;

        QString selectedEncoding() const;
        void updatePreview();
    };

}

#endif // HELLOUTAU_INTERCHANGE_MIDIENCODINGPAGE_H
