#ifndef HELLOUTAU_EDITOR_SETTINGSDIALOG_H
#define HELLOUTAU_EDITOR_SETTINGSDIALOG_H

#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QFormLayout;
class QLineEdit;

namespace hello::daw {

    class AppSettings;

    /// Edits AppSettings. Accepting the dialog writes the settings.
    class HELLOUTAU_EDITOR_EXPORT SettingsDialog : public QDialog {
        Q_OBJECT
    public:
        explicit SettingsDialog(AppSettings &settings, QWidget *parent = nullptr);
        ~SettingsDialog();

        void accept() override;

    private:
        AppSettings &m_settings;
        QLineEdit *m_utauDirectory;
        QLineEdit *m_resampler;
        QLineEdit *m_wavtool;
        QComboBox *m_ustExportCharset;

        // Adds a row with a line edit and a button that browses for a directory or a file.
        QLineEdit *addPathRow(QFormLayout *form, const QString &label, const QString &text,
                              bool directory);
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGSDIALOG_H
