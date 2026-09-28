#ifndef HELLOUTAU_EDITOR_VIBRATODIALOG_H
#define HELLOUTAU_EDITOR_VIBRATODIALOG_H

#include <array>

#include <QtWidgets/QDialog>

#include <hellokit/Document/Note.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QDoubleSpinBox;

namespace hello::daw {

    /// Edits the seven values of a vibrato as UTAU shows them: its length in percent of the
    /// note, its period in milliseconds, its depth in cents, its fading in and out in percent of
    /// its length, its phase in percent of the period, and its height offset in percent of the
    /// depth. The eighth value of \c VBR, which UTAU ignores, is kept as it was.
    ///
    /// Only the values that the user changes are written: a value read from a file with more
    /// decimals than a field shows, or beyond its range, is kept as it was.
    class HELLOUTAU_EDITOR_EXPORT VibratoDialog : public QDialog {
        Q_OBJECT
    public:
        explicit VibratoDialog(const kit::Vibrato &vibrato, QWidget *parent = nullptr);
        ~VibratoDialog();

        kit::Vibrato vibrato() const;

        /// The field of a value, in the order of \c VBR: length, period, depth, fade in, fade
        /// out, phase, height.
        QDoubleSpinBox *field(int index) const;

        /// The vibrato of a note that is given one: \c 65,180,35,20,20,0,0 as in UTAU.
        static kit::Vibrato defaultVibrato();

    private:
        kit::Vibrato m_vibrato;
        std::array<QDoubleSpinBox *, 7> m_fields{};
        // What each field showed at first, by which an unchanged field is recognized
        std::array<double, 7> m_shown{};
    };

}

#endif // HELLOUTAU_EDITOR_VIBRATODIALOG_H
