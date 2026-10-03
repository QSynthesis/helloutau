#ifndef HELLOUTAU_EDITOR_PITCHCONTROLDIALOG_H
#define HELLOUTAU_EDITOR_PITCHCONTROLDIALOG_H

#include <array>
#include <optional>

#include <QtWidgets/QDialog>

#include <hellokit/Document/Note.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QSlider;

namespace hello::daw {

    /// Edits portamento and vibrato as UTAU's Ctrl+T tone-control dialog.
    class HELLOUTAU_EDITOR_EXPORT PitchControlDialog : public QDialog {
        Q_OBJECT
    public:
        PitchControlDialog(std::optional<bool> portamento, std::optional<bool> vibrato,
                           const kit::Vibrato &values, int portamentoPreset = 0,
                           int vibratoPreset = 0, int portamentoMode = 0,
                           int portamentoLength = 59, int portamentoStart = -30,
                           int portamentoCount = 2, bool averagePoints = true,
                           const QList<kit::PortamentoPoint> &existingPortamento = {},
                           int noteLength = 480,
                           QWidget *parent = nullptr);
        explicit PitchControlDialog(const kit::Vibrato &values, QWidget *parent = nullptr);
        ~PitchControlDialog();

        Qt::CheckState portamentoState() const;
        Qt::CheckState vibratoState() const;
        kit::Vibrato vibrato() const;
        bool vibratoEdited() const;
        int portamentoPreset() const;
        int vibratoPreset() const;
        int portamentoMode() const;
        int portamentoLength() const;
        int portamentoStart() const;
        int portamentoCount() const;
        bool averagePoints() const;
        void setPortamentoContext(const QList<kit::PortamentoPoint> &points, int noteLength);
        QList<kit::PortamentoPoint> portamentoPoints() const;
        QPushButton *defaultButton() const;

        /// The field of a value, in the order of \c VBR: length, period, depth, fade in, fade
        /// out, phase, height.
        QDoubleSpinBox *field(int index) const;

        static kit::Vibrato defaultVibrato();

    private:
        kit::Vibrato m_vibrato;
        QCheckBox *m_portamento;
        QCheckBox *m_vibratoBox;
        QComboBox *m_portamentoPreset;
        QComboBox *m_vibratoPreset;
        QRadioButton *m_portamentoCustom;
        QRadioButton *m_portamentoAddPoints;
        QRadioButton *m_portamentoPresetMode;
        QSpinBox *m_portamentoLength;
        QSpinBox *m_portamentoStart;
        QSlider *m_portamentoLengthSlider;
        QSlider *m_portamentoStartSlider;
        QComboBox *m_portamentoCount;
        QCheckBox *m_averagePoints;
        QPushButton *m_default;
        std::array<QDoubleSpinBox *, 7> m_fields{};
        // What each field showed at first, by which an unchanged field is recognized
        std::array<double, 7> m_shown{};
        QList<kit::PortamentoPoint> m_existingPortamento;
        int m_noteLength = 480;
    };

}

#endif // HELLOUTAU_EDITOR_PITCHCONTROLDIALOG_H
