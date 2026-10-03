#ifndef HELLOUTAU_EDITOR_PROJECTPROPERTIESDIALOG_H
#define HELLOUTAU_EDITOR_PROJECTPROPERTIESDIALOG_H

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
    /// Project engines are used only after both programs have been trusted in the application
    /// settings. Otherwise rendering uses the configured defaults.
    class HELLOUTAU_EDITOR_EXPORT ProjectPropertiesDialog : public QDialog {
        Q_OBJECT
    public:
        explicit ProjectPropertiesDialog(const kit::Project &project, QWidget *parent = nullptr);
        ProjectPropertiesDialog(const kit::Project &project, AppSettings &settings,
                                QWidget *parent = nullptr);
        ~ProjectPropertiesDialog();

        /// The fields that differ from the project, for ProjectEdits::setProperties(). The tempo
        /// counts only once it was edited, so that a value the box rounds is not changed.
        kit::ProjectPropertyChanges changes() const;

        QLineEdit *nameEdit() const;
        QDoubleSpinBox *tempoBox() const;
        QLineEdit *flagsEdit() const;
        QLineEdit *outputFileEdit() const;
        QLineEdit *voiceDirEdit() const;
        QLineEdit *wavtoolEdit() const;
        QLineEdit *resamplerEdit() const;
        QCheckBox *mode2Box() const;

    private:
        ProjectPropertiesDialog(const kit::Project &project, AppSettings *settings,
                                QWidget *parent);

        QString voiceDirText() const;
        void setVoiceDirInvalid(bool invalid);
        void setPathInvalid(QLineEdit *edit, bool invalid);

        kit::Project m_project;
        AppSettings *m_appSettings = nullptr;
        QLineEdit *m_name;
        QDoubleSpinBox *m_tempo;
        bool m_tempoEdited = false;
        QLineEdit *m_flags;
        QLineEdit *m_outputFile;
        QComboBox *m_voiceDir;
        QAction *m_voiceDirInvalid;
        QAction *m_wavtoolInvalid;
        QAction *m_resamplerInvalid;
        QLineEdit *m_wavtool;
        QLineEdit *m_resampler;
        QLabel *m_engineWarning;
        QCheckBox *m_mode2;
    };

}

#endif // HELLOUTAU_EDITOR_PROJECTPROPERTIESDIALOG_H
