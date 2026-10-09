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
class QLabel;
class QLineEdit;

namespace hello::daw {

    class AppSettings;

    /// Edits the properties of a project, the fields of the dialog of UTAU without the tempo: the
    /// name, the flags of every note, the output file, the voice folder of the first track, the
    /// two synth tools, and Mode2. The project tempo is edited in the tool bar.
    ///
    /// Rendering uses the two synth tools of the project, each of which must be a synth tool of the
    /// application settings or be trusted. Reset to Settings Defaults copies the synth tools of the
    /// settings into the fields.
    class HELLOUTAU_EDITOR_EXPORT ProjectPropertiesDialog : public QDialog {
        Q_OBJECT
        Q_PROPERTY(QColor trustedColor READ trustedColor WRITE setTrustedColor)
        Q_PROPERTY(QColor untrustedColor READ untrustedColor WRITE setUntrustedColor)
    public:
        /// \a settings provides the UTAU directory, the synth tools of the settings and the trust
        /// records, and records the trust that the user grants in the dialog.
        ProjectPropertiesDialog(const kit::Project &project, AppSettings &settings,
                                QWidget *parent = nullptr);
        ~ProjectPropertiesDialog();

        /// Returns the fields that differ from the project, for ProjectEdits::setProperties().
        ///
        /// If any field differs, the voice folder and the two synth tools are normalized as well
        /// and are returned if their normalized values differ. An absolute voice folder inside a
        /// voice folder of the settings becomes a \c %VOICE% value, as UTAU writes it. A synth tool
        /// inside the UTAU directory becomes relative to that directory, and any other synth tool
        /// becomes an absolute path. All three use the separators of kit::Project::savedPathText().
        /// If no field differs, nothing is normalized and the changes are empty.
        kit::ProjectPropertyChanges changes() const;

        QLineEdit *nameEdit() const;
        QLineEdit *flagsEdit() const;
        QLineEdit *outputFileEdit() const;
        QLineEdit *voiceDirEdit() const;
        QLineEdit *wavtoolEdit() const;
        QLineEdit *resamplerEdit() const;
        QCheckBox *mode2Box() const;

        /// The color of the state of a synth tool that renders, by default dark green. It is set in
        /// a theme with \c qproperty-trustedColor.
        QColor trustedColor() const;
        void setTrustedColor(const QColor &color);

        /// The color of the state of an untrusted synth tool and of the warning about it, by
        /// default dark red. It is set in a theme with \c qproperty-untrustedColor.
        QColor untrustedColor() const;
        void setUntrustedColor(const QColor &color);

    private:
        void showVoiceDir(const QString &voiceDir);
        std::filesystem::path voiceDirectory() const;
        void updateVoiceDirResolved();
        void browseVoiceDir();
        bool checkPaths();
        void updateTrust();
        void trustSynthTools();
        void acceptIfValid();

        kit::Project m_project;
        AppSettings &m_settings;
        QLineEdit *m_name;
        QLineEdit *m_flags;
        QLineEdit *m_outputFile;
        QComboBox *m_voiceDir;
        QAction *m_voiceDirInvalid;
        QLabel *m_voiceDirResolved;
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
