#include "ProjectPropertiesDialog.h"

#include <functional>

#include <QtCore/QDir>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    namespace {

        // The tempo a box offers, which the project may exceed, since UTAU writes any value
        constexpr double MaximumTempo = 1000;

        // A line edit with a button beside it that browses for its value
        QHBoxLayout *withBrowse(QLineEdit *edit, QWidget *parent,
                                std::function<QString(const QString &)> browse) {
            auto button = new QPushButton(ProjectPropertiesDialog::tr("Browse..."));
            QObject::connect(button, &QPushButton::clicked, parent, [edit, browse] {
                const auto chosen = browse(edit->text());
                if (!chosen.isEmpty()) {
                    edit->setText(QDir::toNativeSeparators(chosen));
                }
            });
            auto layout = new QHBoxLayout();
            layout->addWidget(edit, 1);
            layout->addWidget(button);
            return layout;
        }

    }

    ProjectPropertiesDialog::ProjectPropertiesDialog(const kit::Project &project, QWidget *parent)
        : QDialog(parent), m_project(project) {
        setWindowTitle(tr("Project Properties"));
        const auto &settings = project.settings;

        m_name = new QLineEdit(settings.name);
        m_tempo = new QDoubleSpinBox();
        m_tempo->setDecimals(2);
        m_tempo->setRange(0.01, MaximumTempo);
        m_tempo->setValue(settings.tempo);
        connect(m_tempo, &QDoubleSpinBox::valueChanged, this, [this] { m_tempoEdited = true; });
        m_flags = new QLineEdit(settings.flags);
        m_outputFile = new QLineEdit(settings.outputFile);
        m_voiceDir =
            new QLineEdit(project.tracks.isEmpty() ? QString() : project.tracks.first().voiceDir);
        m_voiceDir->setEnabled(!project.tracks.isEmpty());
        m_wavtool = new QLineEdit(settings.wavtool);
        m_resampler = new QLineEdit(settings.resampler);
        m_mode2 = new QCheckBox(tr("Mode&2 pitch"));
        m_mode2->setChecked(settings.mode2);

        auto form = new QFormLayout();
        form->addRow(tr("&Name:"), m_name);
        form->addRow(tr("&Tempo:"), m_tempo);
        form->addRow(tr("&Flags:"), m_flags);
        form->addRow(
            tr("&Voice folder:"), withBrowse(m_voiceDir, this, [this](const QString &current) {
                return QFileDialog::getExistingDirectory(this, tr("Choose Voice Folder"), current);
            }));
        form->addRow(tr("&Output file:"),
                     withBrowse(m_outputFile, this, [this](const QString &current) {
                         return QFileDialog::getSaveFileName(this, tr("Choose Output File"),
                                                             current, tr("Wave files (*.wav)"));
                     }));
        form->addRow(tr("Wav&tool (Tool1):"), m_wavtool);
        form->addRow(tr("&Resampler (Tool2):"), m_resampler);
        auto note = new QLabel(tr("The engines are only recorded in the project. HelloUtau "
                                  "renders with the engines of its settings."));
        note->setWordWrap(true);
        form->addRow(note);
        form->addRow(m_mode2);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
        resize(560, 0);
    }

    ProjectPropertiesDialog::~ProjectPropertiesDialog() = default;

    kit::ProjectPropertyChanges ProjectPropertiesDialog::changes() const {
        const auto &settings = m_project.settings;
        const auto text = [](const QLineEdit *edit, const QString &was) {
            return edit->text() != was ? std::optional(edit->text()) : std::nullopt;
        };
        kit::ProjectPropertyChanges changes;
        changes.name = text(m_name, settings.name);
        if (m_tempoEdited && m_tempo->value() != settings.tempo) {
            changes.tempo = m_tempo->value();
        }
        changes.flags = text(m_flags, settings.flags);
        changes.outputFile = text(m_outputFile, settings.outputFile);
        if (!m_project.tracks.isEmpty()) {
            changes.voiceDir = text(m_voiceDir, m_project.tracks.first().voiceDir);
        }
        changes.wavtool = text(m_wavtool, settings.wavtool);
        changes.resampler = text(m_resampler, settings.resampler);
        if (m_mode2->isChecked() != settings.mode2) {
            changes.mode2 = m_mode2->isChecked();
        }
        return changes;
    }

    QLineEdit *ProjectPropertiesDialog::nameEdit() const {
        return m_name;
    }

    QDoubleSpinBox *ProjectPropertiesDialog::tempoBox() const {
        return m_tempo;
    }

    QLineEdit *ProjectPropertiesDialog::flagsEdit() const {
        return m_flags;
    }

    QLineEdit *ProjectPropertiesDialog::outputFileEdit() const {
        return m_outputFile;
    }

    QLineEdit *ProjectPropertiesDialog::voiceDirEdit() const {
        return m_voiceDir;
    }

    QLineEdit *ProjectPropertiesDialog::wavtoolEdit() const {
        return m_wavtool;
    }

    QLineEdit *ProjectPropertiesDialog::resamplerEdit() const {
        return m_resampler;
    }

    QCheckBox *ProjectPropertiesDialog::mode2Box() const {
        return m_mode2;
    }

}
