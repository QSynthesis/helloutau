#ifndef HELLOUTAU_EDITOR_PASTEPARAMETERSDIALOG_H
#define HELLOUTAU_EDITOR_PASTEPARAMETERSDIALOG_H

#include <QtWidgets/QDialog>

#include <helloutau/Editor/PianoRoll.h>

class QCheckBox;

namespace hello::daw {

    /// Asks which parameters of the copied notes to paste onto the selected ones (as the Paste
    /// Parameters dialog of OpenUtau does). OK is disabled while none is chosen.
    class HELLOUTAU_EDITOR_EXPORT PasteParametersDialog : public QDialog {
        Q_OBJECT
    public:
        explicit PasteParametersDialog(PianoRoll::Parameters parameters, QWidget *parent = nullptr);
        ~PasteParametersDialog();

        PianoRoll::Parameters parameters() const;

        /// The box of \a parameter, one of PortamentoParameter, VibratoParameter and
        /// EnvelopeParameter.
        QCheckBox *box(PianoRoll::Parameter parameter) const;

    private:
        QCheckBox *m_portamento;
        QCheckBox *m_vibrato;
        QCheckBox *m_envelope;
    };

}

#endif // HELLOUTAU_EDITOR_PASTEPARAMETERSDIALOG_H
