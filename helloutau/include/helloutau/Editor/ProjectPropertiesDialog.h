#ifndef HELLOUTAU_EDITOR_PROJECTPROPERTIESDIALOG_H
#define HELLOUTAU_EDITOR_PROJECTPROPERTIESDIALOG_H

#include <QtWidgets/QDialog>

#include <hellokit/Document/Project.h>
#include <hellokit/Edit/ProjectEdits.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;

namespace hello::daw {

    /// Edits the properties of a project, the fields of the dialog of UTAU and the tempo: the
    /// name, the tempo, the flags of every note, the output file, the voice folder of the first
    /// track, the two engines, and Mode2.
    ///
    /// The engines are only recorded in the project: rendering always uses the engines of the
    /// settings, as the dialog states, see the security section of CLAUDE.md.
    class HELLOUTAU_EDITOR_EXPORT ProjectPropertiesDialog : public QDialog {
        Q_OBJECT
    public:
        explicit ProjectPropertiesDialog(const kit::Project &project, QWidget *parent = nullptr);
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
        kit::Project m_project;
        QLineEdit *m_name;
        QDoubleSpinBox *m_tempo;
        bool m_tempoEdited = false;
        QLineEdit *m_flags;
        QLineEdit *m_outputFile;
        QLineEdit *m_voiceDir;
        QLineEdit *m_wavtool;
        QLineEdit *m_resampler;
        QCheckBox *m_mode2;
    };

}

#endif // HELLOUTAU_EDITOR_PROJECTPROPERTIESDIALOG_H
