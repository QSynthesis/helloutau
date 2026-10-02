#include "ProjectPropertiesDialog.h"

#include <functional>
#include <system_error>

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
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QVBoxLayout>

#include "AppSettings.h"
#include "EngineTrust_p.h"

namespace hello::daw {

    namespace {

        constexpr double MinimumTempo = 10;
        constexpr double MaximumTempo = 512;

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
        : ProjectPropertiesDialog(project, nullptr, parent) {
    }

    ProjectPropertiesDialog::ProjectPropertiesDialog(const kit::Project &project,
                                                     AppSettings &appSettings, QWidget *parent)
        : ProjectPropertiesDialog(project, &appSettings, parent) {
    }

    ProjectPropertiesDialog::ProjectPropertiesDialog(const kit::Project &project,
                                                     AppSettings *appSettings, QWidget *parent)
        : QDialog(parent), m_project(project), m_appSettings(appSettings) {
        setWindowTitle(tr("Project Properties"));
        const auto &settings = project.settings;

        m_name = new QLineEdit(settings.name);
        m_tempo = new QDoubleSpinBox();
        m_tempo->setDecimals(2);
        m_tempo->setRange(MinimumTempo, MaximumTempo);
        m_tempo->setValue(settings.tempo);
        connect(m_tempo, &QDoubleSpinBox::valueChanged, this, [this] { m_tempoEdited = true; });
        m_flags = new QLineEdit(settings.flags);
        m_outputFile = new QLineEdit(QDir::toNativeSeparators(settings.outputFile));
        m_voiceDir = new QLineEdit(QDir::toNativeSeparators(
            project.tracks.isEmpty() ? QString() : project.tracks.first().voiceDir));
        m_voiceDir->setEnabled(!project.tracks.isEmpty());
        m_wavtool = new QLineEdit(QDir::toNativeSeparators(settings.wavtool));
        m_resampler = new QLineEdit(QDir::toNativeSeparators(settings.resampler));
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
        auto reset = new QPushButton(tr("Reset to settings defaults"));
        connect(reset, &QPushButton::clicked, this, [this] {
            if (!m_appSettings) {
                return;
            }
            m_wavtool->setText(QDir::toNativeSeparators(m_appSettings->wavtool()));
            m_resampler->setText(QDir::toNativeSeparators(m_appSettings->resampler()));
        });
        reset->setObjectName(QStringLiteral("resetProjectEngines"));
        reset->setEnabled(m_appSettings);
        form->addRow(reset);
        auto trust = new QPushButton(tr("Trust project engines"));
        trust->setEnabled(m_appSettings);
        form->addRow(trust);
        m_engineWarning = new QLabel(
            tr("Warning: project engines are untrusted. Playback does not render until the "
               "required project engines are trusted."));
        m_engineWarning->setWordWrap(true);
        const auto updatePathValidity = [this] {
            if (!m_appSettings) {
                return;
            }
            const auto utau = m_appSettings->utauDirectory();
            const auto markFile = [&utau](QLineEdit *edit) {
                std::error_code error;
                const auto path = EngineTrust::resolved(edit->text(), utau);
                edit->setStyleSheet(!path.empty() && std::filesystem::is_regular_file(path, error)
                                         ? QString()
                                         : QStringLiteral("background: #ffd6d6;"));
            };
            markFile(m_wavtool);
            markFile(m_resampler);
            if (!m_project.tracks.isEmpty()) {
                std::error_code error;
                const auto root = m_project.tracks.first().voiceDirectory(utau);
                m_voiceDir->setStyleSheet(
                    !root.empty() && std::filesystem::is_directory(root, error)
                        ? QString()
                        : QStringLiteral("background: #ffd6d6;"));
            }
        };
        const auto updateTrust = [this] {
            if (!m_appSettings) {
                return;
            }
            const auto utau = m_appSettings->utauDirectory();
            const auto status = [this, &utau](QLineEdit *edit, const QString &role) {
                QString text;
                if (EngineTrust::samePath(edit->text(), m_appSettings->resampler(), utau) ||
                    EngineTrust::samePath(edit->text(), m_appSettings->wavtool(), utau)) {
                    text = tr("Using the default %1 from Settings.").arg(role);
                    return qMakePair(text, false);
                }
                if (EngineTrust::isTrusted(*m_appSettings, edit->text(), utau)) {
                    text = tr("Project %1 is trusted.").arg(role);
                    return qMakePair(text, false);
                }
                text = tr("Project %1 is untrusted.").arg(role);
                return qMakePair(text, true);
            };
            const auto wavtool = status(m_wavtool, tr("wavtool"));
            const auto resampler = status(m_resampler, tr("resampler"));
            const auto line = [](const QPair<QString, bool> &value) {
                return QStringLiteral("<span style=\"color:%1; font-weight:bold;\">%2</span>")
                    .arg(value.second ? QStringLiteral("#b00020") : QStringLiteral("#176b2c"),
                         value.first.toHtmlEscaped());
            };
            m_engineWarning->setText(line(wavtool) + QStringLiteral("<br>") + line(resampler));
            m_engineWarning->setTextFormat(Qt::RichText);
        };
        connect(m_voiceDir, &QLineEdit::textChanged, this, updatePathValidity);
        connect(m_wavtool, &QLineEdit::textChanged, this, updatePathValidity);
        connect(m_resampler, &QLineEdit::textChanged, this, updatePathValidity);
        connect(m_wavtool, &QLineEdit::textChanged, this, updateTrust);
        connect(m_resampler, &QLineEdit::textChanged, this, updateTrust);
        updatePathValidity();
        updateTrust();
        connect(trust, &QPushButton::clicked, this, [this, updateTrust] {
            if (!m_appSettings) {
                return;
            }
            const auto utau = m_appSettings->utauDirectory();
            const auto sameTool =
                EngineTrust::samePath(m_wavtool->text(), m_resampler->text(), utau);
            if (!EngineTrust::samePath(m_wavtool->text(), m_appSettings->wavtool(), utau) &&
                !EngineTrust::samePath(m_wavtool->text(), m_appSettings->resampler(), utau)) {
                EngineTrust::ask(this, *m_appSettings, m_wavtool->text(), utau);
            }
            if (!sameTool &&
                !EngineTrust::samePath(m_resampler->text(), m_appSettings->resampler(), utau) &&
                !EngineTrust::samePath(m_resampler->text(), m_appSettings->wavtool(), utau)) {
                EngineTrust::ask(this, *m_appSettings, m_resampler->text(), utau);
            }
            updateTrust();
        });
        form->addRow(m_engineWarning);
        form->addRow(m_mode2);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            auto invalid = [](QLineEdit *edit) {
                edit->setStyleSheet(QStringLiteral("background: #ffd6d6;"));
            };
            auto valid = [](QLineEdit *edit) { edit->setStyleSheet(QString()); };
            bool ok = true;
            if (m_appSettings) {
                const auto utau = m_appSettings->utauDirectory();
                const auto normalize = [&](QLineEdit *edit) {
                    const auto path = EngineTrust::resolved(edit->text(), utau);
                    std::error_code error;
                    if (path.empty() || !std::filesystem::is_regular_file(path, error)) {
                        invalid(edit);
                        return false;
                    }
                    if (!utau.empty()) {
                        const auto root = std::filesystem::weakly_canonical(utau, error);
                        const auto absolute = std::filesystem::weakly_canonical(path, error);
                        const auto relative = std::filesystem::relative(absolute, root, error);
                        const bool outside = !relative.empty() &&
                                             relative.begin() != relative.end() &&
                                             *relative.begin() == std::filesystem::path("..");
                        if (!error && outside) {
                            edit->setText(QDir::toNativeSeparators(
                                QString::fromStdU16String(absolute.u16string())));
                        } else if (!error && !relative.empty()) {
                            edit->setText(QDir::toNativeSeparators(
                                QString::fromStdU16String(relative.u16string())));
                        }
                    }
                    valid(edit);
                    return true;
                };
                ok = normalize(m_wavtool) && normalize(m_resampler);
                if (!m_project.tracks.isEmpty()) {
                    std::error_code error;
                    const auto root = m_project.tracks.first().voiceDirectory(utau);
                    if (root.empty() || !std::filesystem::is_directory(root, error)) {
                        invalid(m_voiceDir);
                        ok = false;
                    } else {
                        valid(m_voiceDir);
                    }
                }
            }
            if (ok) {
                accept();
            } else {
                QMessageBox::warning(this, tr("Invalid Project Path"),
                                     tr("The voice folder, wavtool, and resampler must exist."));
            }
        });
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
        const auto pathText = [](const QLineEdit *edit, const QString &was) {
            const auto value = QDir::fromNativeSeparators(edit->text());
            return value != QDir::fromNativeSeparators(was) ? std::optional(value) : std::nullopt;
        };
        kit::ProjectPropertyChanges changes;
        changes.name = text(m_name, settings.name);
        if (m_tempoEdited && m_tempo->value() != settings.tempo) {
            changes.tempo = m_tempo->value();
        }
        changes.flags = text(m_flags, settings.flags);
        changes.outputFile = pathText(m_outputFile, settings.outputFile);
        if (!m_project.tracks.isEmpty()) {
            changes.voiceDir = pathText(m_voiceDir, m_project.tracks.first().voiceDir);
        }
        changes.wavtool = pathText(m_wavtool, settings.wavtool);
        changes.resampler = pathText(m_resampler, settings.resampler);
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
