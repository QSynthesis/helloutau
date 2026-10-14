#ifndef HELLOUTAU_EDITOR_DIALOGS_CONVERTFREQUENCYMAPSDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_CONVERTFREQUENCYMAPSDIALOG_H

#include <QtCore/QList>
#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QDialogButtonBox;

namespace hello::kit {
    class FrequencyFormat;
}

namespace hello::daw {

    /// A dialog in which the user chooses the format from which the frequency maps of a number of
    /// audio files are converted and the format into which they are converted. The target formats
    /// are the formats that write maps. The OK button is enabled if the two formats differ.
    class HELLOUTAU_EDITOR_EXPORT ConvertFrequencyMapsDialog : public QDialog {
        Q_OBJECT
    public:
        /// Creates the dialog for the maps of \a count audio files in the formats \a formats,
        /// with \a source as the initial source format. The initial target format is the first
        /// format of \a formats that writes maps and differs from \a source.
        ConvertFrequencyMapsDialog(const QList<kit::FrequencyFormat *> &formats,
                                   const kit::FrequencyFormat *source, int count,
                                   QWidget *parent = nullptr);
        ~ConvertFrequencyMapsDialog();

        /// Returns the source format, or null if \a formats is empty.
        kit::FrequencyFormat *sourceFormat() const;

        /// Returns the target format, or null if no format writes maps.
        kit::FrequencyFormat *targetFormat() const;

        QComboBox *sourceBox() const;
        QComboBox *targetBox() const;
        QDialogButtonBox *buttonBox() const;

    private:
        QList<kit::FrequencyFormat *> m_formats;
        QComboBox *m_source;
        QComboBox *m_target;
        QDialogButtonBox *m_buttons;

        // Enables the OK button if the two formats differ
        void updateButtons();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_CONVERTFREQUENCYMAPSDIALOG_H
