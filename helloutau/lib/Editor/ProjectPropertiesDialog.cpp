#include "ProjectPropertiesDialog.h"

#include <functional>
#include <system_error>

#include <QtCore/QDir>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStyle>
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

        QHBoxLayout *withBrowse(QComboBox *box, QWidget *parent,
                                std::function<QString(const QString &)> browse) {
            auto button = new QPushButton(ProjectPropertiesDialog::tr("Browse..."));
            QObject::connect(button, &QPushButton::clicked, parent, [box, browse] {
                const auto chosen = browse(box->currentText());
                if (!chosen.isEmpty()) {
                    box->setEditText(QDir::toNativeSeparators(chosen));
                }
            });
            auto layout = new QHBoxLayout();
            layout->addWidget(box, 1);
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
        m_voiceDir = new QComboBox();
        m_voiceDir->setEditable(true);
        m_voiceDir->setLineEdit(new QLineEdit());
        m_voiceDir->setInsertPolicy(QComboBox::NoInsert);
        const auto warningIcon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
        m_voiceDirInvalid =
            m_voiceDir->lineEdit()->addAction(warningIcon, QLineEdit::TrailingPosition);
        m_voiceDirInvalid->setToolTip(tr("The path is invalid."));
        m_voiceDirInvalid->setVisible(false);
        const auto voiceValue = QDir::toNativeSeparators(
            project.tracks.isEmpty() ? QString() : project.tracks.first().voiceDir);
        m_voiceDir->setEditText(voiceValue);
        if (m_appSettings) {
            const auto utau = m_appSettings->utauDirectory();
            const auto voiceRoot = utau / u"voice";
            const auto rootText = QString::fromStdU16String(voiceRoot.u16string());
            const auto folders =
                QDir(rootText).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const auto &folder : folders) {
                const auto relative = folder.fileName();
                m_voiceDir->addItem(relative, QStringLiteral("%VOICE%") + relative);
            }
            auto comboValue = voiceValue;
            auto displayValue = voiceValue;
            if (!voiceValue.isEmpty() && !utau.empty()) {
                kit::Track track;
                track.voiceDir = voiceValue;
                const auto directory = track.voiceDirectory(utau);
                if (!directory.empty()) {
                    comboValue = kit::Track::voiceDirOf(directory, utau);
                    if (comboValue.startsWith(kit::Track::voicePrefix)) {
                        displayValue = comboValue.mid(kit::Track::voicePrefix.size());
                    }
                }
            }
            int index = m_voiceDir->findData(comboValue);
            if (index < 0 && comboValue != voiceValue && !displayValue.isEmpty()) {
                m_voiceDir->addItem(displayValue, comboValue);
                index = m_voiceDir->count() - 1;
            }
            {
                const QSignalBlocker blocker(m_voiceDir);
                m_voiceDir->setCurrentIndex(index);
                m_voiceDir->setEditText(index >= 0 ? m_voiceDir->itemText(index) : displayValue);
            }
        }
        m_voiceDir->setEnabled(!project.tracks.isEmpty());
        m_wavtool = new QLineEdit(QDir::toNativeSeparators(settings.wavtool));
        m_resampler = new QLineEdit(QDir::toNativeSeparators(settings.resampler));
        m_wavtoolInvalid = m_wavtool->addAction(warningIcon, QLineEdit::TrailingPosition);
        m_resamplerInvalid = m_resampler->addAction(warningIcon, QLineEdit::TrailingPosition);
        m_wavtoolInvalid->setToolTip(tr("The path is invalid."));
        m_resamplerInvalid->setToolTip(tr("The path is invalid."));
        m_wavtoolInvalid->setVisible(false);
        m_resamplerInvalid->setVisible(false);
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
            const auto markFile = [this, &utau](QLineEdit *edit) {
                std::error_code error;
                const auto path = EngineTrust::resolved(edit->text(), utau);
                setPathInvalid(edit,
                               path.empty() || !std::filesystem::is_regular_file(path, error));
            };
            markFile(m_wavtool);
            markFile(m_resampler);
            if (!m_project.tracks.isEmpty()) {
                std::error_code error;
                auto track = m_project.tracks.first();
                track.voiceDir = voiceDirText();
                const auto root = track.voiceDirectory(utau);
                setVoiceDirInvalid(root.empty() || !std::filesystem::is_directory(root, error));
            }
        };
        const auto updateTrust = [this] {
            if (!m_appSettings) {
                return;
            }
            const auto utau = m_appSettings->utauDirectory();
            const auto status = [this, &utau](QLineEdit *edit, const QString &role,
                                              const QString &defaultPath, const QString &otherPath,
                                              const QString &otherRole) {
                QString text;
                if (EngineTrust::samePath(edit->text(), defaultPath, utau)) {
                    text = tr("Using the default %1 from Settings.").arg(role);
                    return qMakePair(text, false);
                }
                if (EngineTrust::samePath(edit->text(), otherPath, utau)) {
                    text = tr("Using the Settings %1 as the project %2.").arg(otherRole, role);
                    return qMakePair(text, false);
                }
                if (EngineTrust::isTrusted(*m_appSettings, edit->text(), utau)) {
                    text = tr("Project %1 is trusted.").arg(role);
                    return qMakePair(text, false);
                }
                text = tr("Project %1 is untrusted.").arg(role);
                return qMakePair(text, true);
            };
            const auto wavtool = status(m_wavtool, tr("wavtool"), m_appSettings->wavtool(),
                                        m_appSettings->resampler(), tr("resampler"));
            const auto resampler = status(m_resampler, tr("resampler"), m_appSettings->resampler(),
                                          m_appSettings->wavtool(), tr("wavtool"));
            const auto line = [](const QPair<QString, bool> &value) {
                return QStringLiteral("<span style=\"color:%1; font-weight:bold;\">%2</span>")
                    .arg(value.second ? QStringLiteral("#b00020") : QStringLiteral("#176b2c"),
                         value.first.toHtmlEscaped());
            };
            m_engineWarning->setText(line(wavtool) + QStringLiteral("<br>") + line(resampler));
            m_engineWarning->setTextFormat(Qt::RichText);
        };
        connect(m_voiceDir->lineEdit(), &QLineEdit::textChanged, this, updatePathValidity);
        connect(m_voiceDir, &QComboBox::currentIndexChanged, this, updatePathValidity);
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
            auto invalid = [this](QLineEdit *edit) { setPathInvalid(edit, true); };
            auto valid = [this](QLineEdit *edit) { setPathInvalid(edit, false); };
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
                    kit::Track track = m_project.tracks.first();
                    track.voiceDir = voiceDirText();
                    std::error_code error;
                    const auto root = track.voiceDirectory(utau);
                    if (root.empty() || !std::filesystem::is_directory(root, error)) {
                        setVoiceDirInvalid(true);
                        ok = false;
                    } else {
                        setVoiceDirInvalid(false);
                    }
                }
            }
            if (ok) {
                accept();
            } else {
                QMessageBox::warning(this, tr("Invalid Project Path"),
                                     tr("The voice folder, wavtool, and resampler must be valid."));
            }
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
        resize(560, 0);
    }

    ProjectPropertiesDialog::~ProjectPropertiesDialog() = default;

    QString ProjectPropertiesDialog::voiceDirText() const {
        const auto edit = m_voiceDir->lineEdit();
        for (int index = 0; index < m_voiceDir->count(); ++index) {
            if (edit->text() == m_voiceDir->itemText(index)) {
                return m_voiceDir->itemData(index).toString();
            }
        }
        const auto text = edit->text();
        if (m_appSettings && !text.isEmpty() && !text.startsWith(kit::Track::voicePrefix) &&
            !QDir::isAbsolutePath(text)) {
            return kit::Track::voicePrefix.toString() + QDir::toNativeSeparators(text);
        }
        return text;
    }

    void ProjectPropertiesDialog::setVoiceDirInvalid(bool invalid) {
        m_voiceDirInvalid->setVisible(invalid);
    }

    void ProjectPropertiesDialog::setPathInvalid(QLineEdit *edit, bool invalid) {
        const auto action = edit == m_wavtool ? m_wavtoolInvalid : m_resamplerInvalid;
        action->setVisible(invalid);
    }

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
            const auto voice = voiceDirText();
            auto sameDirectory = voice == m_project.tracks.first().voiceDir;
            if (!sameDirectory && m_appSettings) {
                kit::Track before;
                before.voiceDir = m_project.tracks.first().voiceDir;
                kit::Track after;
                after.voiceDir = voice;
                const auto utau = m_appSettings->utauDirectory();
                std::error_code error;
                const auto beforePath = before.voiceDirectory(utau);
                const auto afterPath = after.voiceDirectory(utau);
                sameDirectory = !beforePath.empty() && !afterPath.empty() &&
                                std::filesystem::weakly_canonical(beforePath, error) ==
                                    std::filesystem::weakly_canonical(afterPath, error) && !error;
            }
            if (!sameDirectory) {
                changes.voiceDir = voice;
            }
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
        return m_voiceDir->lineEdit();
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
