#ifndef HELLOUTAU_EDITOR_DIALOGS_PITCHCONTROLDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_PITCHCONTROLDIALOG_H

#include <array>
#include <optional>

#include <QtWidgets/QDialog>

#include <hellokit/Document/Note.h>
#include <hellokit/Document/PortamentoSettings.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QSlider;

namespace hello::daw {

    /// Edits the portamento and the vibrato of the selected notes, after the tone control dialog
    /// of UTAU (Ctrl+T). Each group has a check box, partially checked if the selected notes
    /// differ, and the fields of the group are enabled only while it is checked.
    class HELLOUTAU_EDITOR_EXPORT PitchControlDialog : public QDialog {
        Q_OBJECT
    public:
        /// The selected notes and the defaults that the dialog shows.
        struct Selection {
            /// Whether the notes all have, all lack or differ in portamento
            std::optional<bool> portamento;
            /// Whether the notes all have, all lack or differ in vibrato
            std::optional<bool> vibrato;

            /// The vibrato shown, that of the first note or the default
            kit::Vibrato vibratoValues = kit::Vibrato::utauDefault();
            /// The index of the vibrato preset shown
            int vibratoPreset = 0;

            kit::PortamentoSettings portamentoSettings;

            /// The Mode2 points and the duration in milliseconds of the first note, from which
            /// portamentoPoints() derives the points
            QList<kit::PortamentoPoint> points;
            double duration = 0;

            /// The duration in milliseconds of the note before the first note, which bounds how
            /// far the custom portamento starts before the note, or \c std::nullopt if none
            std::optional<double> previousDuration;
        };

        explicit PitchControlDialog(const Selection &selection, QWidget *parent = nullptr);
        ~PitchControlDialog();

        Qt::CheckState portamentoState() const;
        Qt::CheckState vibratoState() const;

        /// Returns the vibrato shown, in which only the fields that were edited differ from the
        /// vibrato of the selection, so that the decimals of the other fields remain.
        kit::Vibrato vibrato() const;

        /// Returns whether a field of the vibrato was edited.
        bool vibratoEdited() const;

        /// Returns whether the check box or a setting of the portamento group was changed.
        bool portamentoEdited() const;

        /// Returns the settings of the portamento group as shown.
        kit::PortamentoSettings portamentoSettings() const;

        int vibratoPreset() const;

        /// Returns the points that the settings give the first note, see
        /// kit::PortamentoSettings::pointsFor().
        QList<kit::PortamentoPoint> portamentoPoints() const;

        /// The Set as Default buttons, which the caller connects to the settings.
        QPushButton *portamentoDefaultButton() const;
        QPushButton *vibratoDefaultButton() const;

        /// The field of a value, in the order of \c VBR: length, period, depth, fade in, fade
        /// out, phase, height.
        QDoubleSpinBox *field(int index) const;

    private:
        kit::Vibrato m_vibrato;
        QList<kit::PortamentoPoint> m_points;
        double m_duration;
        kit::PortamentoSettings m_initialPortamento;
        Qt::CheckState m_initialPortamentoState;

        QCheckBox *m_portamento;
        QCheckBox *m_vibratoBox;
        QComboBox *m_portamentoPreset;
        QComboBox *m_vibratoPreset;
        QRadioButton *m_portamentoPresetMode;
        QRadioButton *m_portamentoCustom;
        QRadioButton *m_portamentoAddPoints;
        QSpinBox *m_portamentoLength;
        QSpinBox *m_portamentoStart;
        QSlider *m_portamentoLengthSlider;
        QSlider *m_portamentoStartSlider;
        QComboBox *m_portamentoCount;
        QCheckBox *m_averagePoints;
        QPushButton *m_portamentoDefault;
        QPushButton *m_vibratoDefault;
        std::array<QSlider *, 3> m_vibratoSliders{};
        std::array<QDoubleSpinBox *, 7> m_fields{};
        // The value that each field showed at first, by which an unchanged field is recognized
        std::array<double, 7> m_shown{};
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_PITCHCONTROLDIALOG_H
