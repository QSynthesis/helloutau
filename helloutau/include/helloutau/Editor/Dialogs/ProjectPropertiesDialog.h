#ifndef HELLOUTAU_EDITOR_DIALOGS_PROJECTPROPERTIESDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_PROJECTPROPERTIESDIALOG_H

#include <filesystem>

#include <QtGui/QColor>
#include <QtWidgets/QDialog>

#include <hellokit/Document/Project.h>
#include <hellokit/Edit/ProjectEdits.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

namespace hello::daw {

    class AppSettings;

    /// Edits the properties of a project, the fields of the dialog of UTAU and the tempo: the
    /// name, the tempo, the flags of every note, the output file, the voice folder of the first
    /// track, the two engines, and Mode2.
    ///
    /// Rendering uses the two engines of the project, each of which must be an engine of the
    /// application settings or be trusted. Reset to Settings Defaults copies the engines of the
    /// settings into the fields.
    class HELLOUTAU_EDITOR_EXPORT ProjectPropertiesDialog : public QDialog {
        Q_OBJECT
        Q_PROPERTY(QColor trustedColor READ trustedColor WRITE setTrustedColor)
        Q_PROPERTY(QColor untrustedColor READ untrustedColor WRITE setUntrustedColor)
    public:
        /// \a settings provides the UTAU directory, the engines of the settings and the trust
        /// records, and records the trust that the user grants in the dialog.
        ProjectPropertiesDialog(const kit::Project &project, AppSettings &settings,
                                QWidget *parent = nullptr);
        ~ProjectPropertiesDialog();

        /// The fields that differ from the project, for ProjectEdits::setProperties(). The tempo
        /// counts only once it was edited, so that a value the box rounds is not changed. An
        /// engine inside the UTAU directory is given relative to that directory, and any other
        /// engine as an absolute path.
        kit::ProjectPropertyChanges changes() const;

        QLineEdit *nameEdit() const;
        QDoubleSpinBox *tempoBox() const;
        QLineEdit *flagsEdit() const;
        QLineEdit *outputFileEdit() const;
        QLineEdit *voiceDirEdit() const;
        QLineEdit *wavtoolEdit() const;
        QLineEdit *resamplerEdit() const;
        QCheckBox *mode2Box() const;

        /// The color of the state of an engine that renders, by default dark green. A theme sets
        /// it with \c qproperty-trustedColor.
        QColor trustedColor() const;
        void setTrustedColor(const QColor &color);

        /// The color of the state of an untrusted engine and of the warning about it, by default
        /// dark red. A theme sets it with \c qproperty-untrustedColor.
        QColor untrustedColor() const;
        void setUntrustedColor(const QColor &color);

    private:
        void showVoiceDir(const QString &voiceDir);
        QString voiceDirText() const;
        std::filesystem::path voiceDirectory() const;
        void browseVoiceDir();
        bool checkPaths();
        void updateTrust();
        void trustEngines();
        void acceptIfValid();

        kit::Project m_project;
        AppSettings &m_settings;
        QLineEdit *m_name;
        QDoubleSpinBox *m_tempo;
        bool m_tempoEdited = false;
        QLineEdit *m_flags;
        QLineEdit *m_outputFile;
        QComboBox *m_voiceDir;
        QAction *m_voiceDirInvalid;
        QLineEdit *m_wavtool;
        QAction *m_wavtoolInvalid;
        QLineEdit *m_resampler;
        QAction *m_resamplerInvalid;
        QLabel *m_wavtoolTrust;
        QLabel *m_resamplerTrust;
        QWidget *m_untrustedNote;
        QLabel *m_untrustedText;
        QCheckBox *m_mode2;
        QColor m_trustedColor = QColor(0x17, 0x6b, 0x2c);
        QColor m_untrustedColor = QColor(0xb0, 0x00, 0x20);
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_PROJECTPROPERTIESDIALOG_H
