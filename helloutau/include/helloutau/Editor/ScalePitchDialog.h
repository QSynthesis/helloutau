#ifndef HELLOUTAU_EDITOR_SCALEPITCHDIALOG_H
#define HELLOUTAU_EDITOR_SCALEPITCHDIALOG_H

#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QDoubleSpinBox;

namespace hello::daw {

    /// Asks the factors, in percent, by which to scale the heights of the pitch points and the
    /// depth of the vibrato of the selected notes. Both start at 100%.
    class HELLOUTAU_EDITOR_EXPORT ScalePitchDialog : public QDialog {
        Q_OBJECT
    public:
        explicit ScalePitchDialog(QWidget *parent = nullptr);
        ~ScalePitchDialog();

        /// The factor of the heights of the points, 1 for 100%
        double portamento() const;

        /// The factor of the depth of the vibrato, 1 for 100%
        double vibrato() const;

        QDoubleSpinBox *portamentoBox() const;
        QDoubleSpinBox *vibratoBox() const;

    private:
        QDoubleSpinBox *m_portamento;
        QDoubleSpinBox *m_vibrato;
    };

}

#endif // HELLOUTAU_EDITOR_SCALEPITCHDIALOG_H
