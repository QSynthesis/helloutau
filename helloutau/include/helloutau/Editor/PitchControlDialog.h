#ifndef HELLOUTAU_EDITOR_PITCHCONTROLDIALOG_H
#define HELLOUTAU_EDITOR_PITCHCONTROLDIALOG_H

#include <array>
#include <optional>

#include <QtWidgets/QDialog>

#include <hellokit/Document/Note.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QDoubleSpinBox;
class QCheckBox;

namespace hello::daw {

    /// Edits portamento and vibrato as UTAU's Ctrl+T tone-control dialog.
    class HELLOUTAU_EDITOR_EXPORT PitchControlDialog : public QDialog {
        Q_OBJECT
    public:
        PitchControlDialog(std::optional<bool> portamento, std::optional<bool> vibrato,
                           const kit::Vibrato &values, QWidget *parent = nullptr);
        explicit PitchControlDialog(const kit::Vibrato &values, QWidget *parent = nullptr);
        ~PitchControlDialog();

        Qt::CheckState portamentoState() const;
        Qt::CheckState vibratoState() const;
        kit::Vibrato vibrato() const;
        bool vibratoEdited() const;

        /// The field of a value, in the order of \c VBR: length, period, depth, fade in, fade
        /// out, phase, height.
        QDoubleSpinBox *field(int index) const;

        static kit::Vibrato defaultVibrato();

    private:
        kit::Vibrato m_vibrato;
        QCheckBox *m_portamento;
        QCheckBox *m_vibratoBox;
        std::array<QDoubleSpinBox *, 7> m_fields{};
        // What each field showed at first, by which an unchanged field is recognized
        std::array<double, 7> m_shown{};
    };

}

#endif // HELLOUTAU_EDITOR_PITCHCONTROLDIALOG_H
