#include "ProjectPropertiesDialog.h"

#include <functional>
#include <system_error>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QVBoxLayout>

#include "AppSettings.h"
#include "SynthToolTrust.h"

namespace hello::daw {

    namespace fs = std::filesystem;

    namespace {

        QString textOf(const fs::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        // The rich text of a label that shows text in color, in bold if bold is true
        QString colored(const QString &text, const QColor &color, bool bold) {
            return QStringLiteral("<span style=\"color:%1;%2\">%3</span>")
                .arg(color.name(), bold ? QStringLiteral(" font-weight:bold;") : QString(),
                     text.toHtmlEscaped());
        }

        // A field with a button beside it that opens a file dialog for the value
        QHBoxLayout *withBrowse(QWidget *field, QWidget *parent, std::function<void()> browse) {
            auto button = new QPushButton(ProjectPropertiesDialog::tr("Browse..."));
            QObject::connect(button, &QPushButton::clicked, parent, std::move(browse));
            auto layout = new QHBoxLayout();
            layout->addWidget(field, 1);
            layout->addWidget(button);
            return layout;
        }

        // A warning icon at the end of edit, shown while the path in edit is invalid
        QAction *addInvalidMark(QLineEdit *edit, const QIcon &icon) {
            const auto action = edit->addAction(icon, QLineEdit::TrailingPosition);
            action->setToolTip(ProjectPropertiesDialog::tr("The path is invalid."));
            action->setVisible(false);
            return action;
        }

        // Returns the synth tool value relative to utau if the synth tool is inside utau, or else
        // as an absolute path. A value that does not name an existing synth tool is returned
        // unchanged.
        QString normalizedSynthTool(const QString &value, const fs::path &utau) {
            if (utau.empty() || !SynthToolTrust::exists(value, utau)) {
                return value;
            }
            std::error_code rootError;
            std::error_code synthToolError;
            const auto root = fs::weakly_canonical(utau, rootError);
            const auto synthTool =
                fs::weakly_canonical(SynthToolTrust::resolved(value, utau), synthToolError);
            if (rootError || synthToolError) {
                return value;
            }
            const auto relative = synthTool.lexically_relative(root);
            const bool inside = !relative.empty() && *relative.begin() != u"..";
            return textOf(inside ? relative : synthTool);
        }

        // Returns the voiceDir value as UTAU writes it on save. An absolute path inside a voice
        // folder of locations becomes a %VOICE% value. A %VOICE% value and a relative path are
        // returned unchanged, see docs/claude/utau-voicedir-cachedir.md.
        QString normalizedVoiceDir(const QString &value, const kit::VoiceLocations &locations) {
            const auto path = kit::Project::pathOf(value);
            if (value.startsWith(kit::Track::voicePrefix) || !path.is_absolute()) {
                return value;
            }
            return kit::Track::voiceDirOf(path, locations);
        }

    }

    ProjectPropertiesDialog::ProjectPropertiesDialog(const kit::Project &project,
                                                     AppSettings &settings, QWidget *parent)
        : QDialog(parent), m_project(project), m_settings(settings) {
        setWindowTitle(tr("Project Properties"));
        const auto &values = project.settings;
        const auto warningIcon = style()->standardIcon(QStyle::SP_MessageBoxWarning);

        m_name = new QLineEdit(values.name);
        m_flags = new QLineEdit(values.flags);
        m_outputFile = new QLineEdit(QDir::toNativeSeparators(values.outputFile));

        // The box holds the voiceDir value itself, so that a relative path and a %VOICE% value
        // of the same name are distinct. The folders in the voice folders are listed by the
        // value that UTAU writes for them. A folder hidden by a folder of the same name in a
        // voice folder of higher priority is therefore listed by its absolute path. The
        // completer matches any part of an item, so that a voice bank is found by its name.
        m_voiceDir = new QComboBox();
        m_voiceDir->setEditable(true);
        m_voiceDir->setInsertPolicy(QComboBox::NoInsert);
        m_voiceDirInvalid = addInvalidMark(m_voiceDir->lineEdit(), warningIcon);
        const auto locations = m_settings.voiceLocations();
        for (const auto &root : locations.voiceFolders) {
            const auto folders =
                QDir(textOf(root)).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const auto &folder : folders) {
                const auto value = kit::Track::voiceDirOf(
                    fs::path(folder.absoluteFilePath().toStdU16String()), locations);
                if (m_voiceDir->findText(value) < 0) {
                    m_voiceDir->addItem(value);
                }
            }
        }
        const auto completer = m_voiceDir->completer();
        completer->setCompletionMode(QCompleter::PopupCompletion);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        m_voiceDirResolved = new QLabel();
        m_voiceDirResolved->setWordWrap(true);
        m_voiceDirResolved->setTextInteractionFlags(Qt::TextSelectableByMouse);
        showVoiceDir(project.tracks.isEmpty()
                         ? QString()
                         : QDir::toNativeSeparators(project.tracks.first().voiceDir));
        m_voiceDir->setEnabled(!project.tracks.isEmpty());

        m_wavtool = new QLineEdit(QDir::toNativeSeparators(values.wavtool));
        m_wavtoolInvalid = addInvalidMark(m_wavtool, warningIcon);
        m_resampler = new QLineEdit(QDir::toNativeSeparators(values.resampler));
        m_resamplerInvalid = addInvalidMark(m_resampler, warningIcon);
        m_mode2 = new QCheckBox(tr("Mode&2 pitch"));
        m_mode2->setChecked(values.mode2);

        // A relative synth tool path is relative to the UTAU directory, which is therefore where
        // the file dialog starts.
        const auto synthToolBrowser = [this](QLineEdit *edit, const QString &title) {
            return [this, edit, title] {
                const auto start =
                    SynthToolTrust::resolved(edit->text(), m_settings.utauDirectory());
                const auto chosen = QFileDialog::getOpenFileName(this, title, textOf(start));
                if (!chosen.isEmpty()) {
                    edit->setText(QDir::toNativeSeparators(chosen));
                }
            };
        };

        auto reset = new QPushButton(tr("Reset to settings defaults"));
        reset->setObjectName(QStringLiteral("resetProjectSynthTools"));
        connect(reset, &QPushButton::clicked, this, [this] {
            m_wavtool->setText(QDir::toNativeSeparators(m_settings.wavtool()));
            m_resampler->setText(QDir::toNativeSeparators(m_settings.resampler()));
        });
        auto trust = new QPushButton(tr("Trust project synth tools"));
        connect(trust, &QPushButton::clicked, this, &ProjectPropertiesDialog::trustSynthTools);

        m_wavtoolTrust = new QLabel();
        m_wavtoolTrust->setWordWrap(true);
        m_wavtoolTrust->setTextFormat(Qt::RichText);
        m_resamplerTrust = new QLabel();
        m_resamplerTrust->setWordWrap(true);
        m_resamplerTrust->setTextFormat(Qt::RichText);
        m_untrustedNote = new QWidget();
        {
            const auto size = style()->pixelMetric(QStyle::PM_SmallIconSize);
            auto icon = new QLabel();
            icon->setPixmap(warningIcon.pixmap(size, size));
            m_untrustedText = new QLabel();
            m_untrustedText->setWordWrap(true);
            m_untrustedText->setTextFormat(Qt::RichText);
            auto layout = new QHBoxLayout(m_untrustedNote);
            layout->setContentsMargins({});
            layout->addWidget(icon, 0, Qt::AlignTop);
            layout->addWidget(m_untrustedText, 1);
        }

        auto form = new QFormLayout();
        form->addRow(tr("&Name:"), m_name);
        form->addRow(tr("&Flags:"), m_flags);
        // The resolved path shares the cell of the voice folder, without the spacing of a row.
        auto voiceField = new QVBoxLayout();
        voiceField->setSpacing(2);
        voiceField->addLayout(withBrowse(m_voiceDir, this, [this] { browseVoiceDir(); }));
        voiceField->addWidget(m_voiceDirResolved);
        form->addRow(tr("&Voice folder:"), voiceField);
        form->addRow(tr("&Output file:"), withBrowse(m_outputFile, this, [this] {
                         const auto chosen = QFileDialog::getSaveFileName(
                             this, tr("Choose Output File"), m_outputFile->text(),
                             tr("Wave files (*.wav)"));
                         if (!chosen.isEmpty()) {
                             m_outputFile->setText(QDir::toNativeSeparators(chosen));
                         }
                     }));
        form->addRow(
            tr("Wav&tool (Tool1):"),
            withBrowse(m_wavtool, this, synthToolBrowser(m_wavtool, tr("Choose Wavtool"))));
        form->addRow(
            tr("&Resampler (Tool2):"),
            withBrowse(m_resampler, this, synthToolBrowser(m_resampler, tr("Choose Resampler"))));
        form->addRow(reset);
        form->addRow(trust);
        form->addRow(m_wavtoolTrust);
        form->addRow(m_resamplerTrust);
        form->addRow(m_untrustedNote);
        form->addRow(m_mode2);

        connect(m_voiceDir->lineEdit(), &QLineEdit::textChanged, this,
                &ProjectPropertiesDialog::checkPaths);
        connect(m_voiceDir->lineEdit(), &QLineEdit::textChanged, this,
                &ProjectPropertiesDialog::updateVoiceDirResolved);
        for (const auto edit : {m_wavtool, m_resampler}) {
            connect(edit, &QLineEdit::textChanged, this, &ProjectPropertiesDialog::checkPaths);
            connect(edit, &QLineEdit::textChanged, this, &ProjectPropertiesDialog::updateTrust);
        }
        checkPaths();
        updateVoiceDirResolved();
        updateTrust();

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this,
                &ProjectPropertiesDialog::acceptIfValid);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
        resize(600, 0);
    }

    ProjectPropertiesDialog::~ProjectPropertiesDialog() = default;

    void ProjectPropertiesDialog::showVoiceDir(const QString &voiceDir) {
        const auto index = m_voiceDir->findText(voiceDir);
        m_voiceDir->setCurrentIndex(index);
        m_voiceDir->setEditText(voiceDir);
    }

    fs::path ProjectPropertiesDialog::voiceDirectory() const {
        kit::Track track;
        track.voiceDir = m_voiceDir->lineEdit()->text();
        return track.voiceDirectory(m_settings.voiceLocations());
    }

    void ProjectPropertiesDialog::updateVoiceDirResolved() {
        const auto directory = voiceDirectory();
        m_voiceDirResolved->setText(
            directory.empty() ? QString() : tr("Resolves to %1").arg(textOf(directory)));
        m_voiceDirResolved->setVisible(!directory.empty());
    }

    void ProjectPropertiesDialog::browseVoiceDir() {
        // The file dialog starts in the current voice bank, or else in the first voice folder
        // that exists, or else in the folder of this program.
        const auto locations = m_settings.voiceLocations();
        std::error_code error;
        auto start = voiceDirectory();
        if (start.empty() || !fs::is_directory(start, error)) {
            start.clear();
            for (const auto &folder : locations.voiceFolders) {
                if (fs::is_directory(folder, error)) {
                    start = folder;
                    break;
                }
            }
        }
        if (start.empty()) {
            start = fs::path(QCoreApplication::applicationDirPath().toStdU16String());
        }
        const auto chosen =
            QFileDialog::getExistingDirectory(this, tr("Choose Voice Folder"), textOf(start));
        if (!chosen.isEmpty()) {
            showVoiceDir(kit::Track::voiceDirOf(fs::path(chosen.toStdU16String()), locations));
        }
    }

    bool ProjectPropertiesDialog::checkPaths() {
        const auto utau = m_settings.utauDirectory();
        const bool wavtool = SynthToolTrust::exists(m_wavtool->text(), utau);
        const bool resampler = SynthToolTrust::exists(m_resampler->text(), utau);
        bool voice = true;
        if (!m_project.tracks.isEmpty()) {
            std::error_code error;
            const auto directory = voiceDirectory();
            voice = !directory.empty() && fs::is_directory(directory, error);
        }
        m_wavtoolInvalid->setVisible(!wavtool);
        m_resamplerInvalid->setVisible(!resampler);
        m_voiceDirInvalid->setVisible(!voice);
        return wavtool && resampler && voice;
    }

    void ProjectPropertiesDialog::updateTrust() {
        // One complete sentence for each synth tool and state, so that a translation is not
        // assembled from fragments
        struct Texts {
            QString same;
            QString swapped;
            QString trusted;
            QString untrusted;
        };
        const auto utau = m_settings.utauDirectory();
        bool untrusted = false;
        const auto describe = [&](QLabel *label, const QString &value, const QString &own,
                                  const QString &other, const Texts &texts) {
            if (SynthToolTrust::samePath(value, own, utau)) {
                label->setText(colored(texts.same, m_trustedColor, true));
            } else if (SynthToolTrust::samePath(value, other, utau)) {
                label->setText(colored(texts.swapped, m_trustedColor, true));
            } else if (SynthToolTrust::isTrusted(m_settings, value, utau)) {
                label->setText(colored(texts.trusted, m_trustedColor, true));
            } else {
                label->setText(colored(texts.untrusted, m_untrustedColor, true));
                untrusted = true;
            }
        };
        describe(m_wavtoolTrust, m_wavtool->text(), m_settings.wavtool(), m_settings.resampler(),
                 {tr("The project uses the wavtool from the settings."),
                  tr("The project uses the resampler from the settings as its wavtool."),
                  tr("The project wavtool is trusted."), tr("The project wavtool is untrusted.")});
        describe(
            m_resamplerTrust, m_resampler->text(), m_settings.resampler(), m_settings.wavtool(),
            {tr("The project uses the resampler from the settings."),
             tr("The project uses the wavtool from the settings as its resampler."),
             tr("The project resampler is trusted."), tr("The project resampler is untrusted.")});
        m_untrustedText->setText(
            colored(tr("Warning: project synth tools are untrusted. Playback does not render until "
                       "the required project synth tools are trusted."),
                    m_untrustedColor, false));
        m_untrustedNote->setVisible(untrusted);
    }

    QColor ProjectPropertiesDialog::trustedColor() const {
        return m_trustedColor;
    }

    void ProjectPropertiesDialog::setTrustedColor(const QColor &color) {
        m_trustedColor = color;
        updateTrust();
    }

    QColor ProjectPropertiesDialog::untrustedColor() const {
        return m_untrustedColor;
    }

    void ProjectPropertiesDialog::setUntrustedColor(const QColor &color) {
        m_untrustedColor = color;
        updateTrust();
    }

    void ProjectPropertiesDialog::trustSynthTools() {
        // A synth tool that does not exist is marked invalid and is not asked about.
        const auto utau = m_settings.utauDirectory();
        QStringList values;
        for (const auto edit : {m_wavtool, m_resampler}) {
            if (SynthToolTrust::exists(edit->text(), utau)) {
                values.push_back(edit->text());
            }
        }
        SynthToolTrust::ask(this, m_settings, values, utau);
        updateTrust();
    }

    void ProjectPropertiesDialog::acceptIfValid() {
        // Without changes, the dialog is accepted even if a path is invalid, and the project is
        // not modified. Any change requires all paths to be valid.
        if (changes().isEmpty() || checkPaths()) {
            accept();
            return;
        }
        QMessageBox::warning(this, tr("Invalid Project Path"),
                             tr("The voice folder, wavtool, and resampler must be valid."));
    }

    kit::ProjectPropertyChanges ProjectPropertiesDialog::changes() const {
        const auto &values = m_project.settings;
        const auto utau = m_settings.utauDirectory();
        const auto text = [](const QLineEdit *edit, const QString &was) {
            return edit->text() != was ? std::optional(edit->text()) : std::nullopt;
        };
        const auto pathText = [](const QString &edited, const QString &was) {
            const auto value = QDir::fromNativeSeparators(edited);
            return value != QDir::fromNativeSeparators(was) ? std::optional(value) : std::nullopt;
        };
        const bool hasTrack = !m_project.tracks.isEmpty();
        const auto voiceBefore = hasTrack ? m_project.tracks.first().voiceDir : QString();
        const auto voice = m_voiceDir->lineEdit()->text();
        kit::ProjectPropertyChanges changes;
        changes.name = text(m_name, values.name);
        changes.flags = text(m_flags, values.flags);
        changes.outputFile = pathText(m_outputFile->text(), values.outputFile);
        if (hasTrack) {
            changes.voiceDir = pathText(voice, voiceBefore);
        }
        changes.wavtool = pathText(m_wavtool->text(), values.wavtool);
        changes.resampler = pathText(m_resampler->text(), values.resampler);
        if (m_mode2->isChecked() != values.mode2) {
            changes.mode2 = m_mode2->isChecked();
        }
        if (changes.isEmpty()) {
            return changes;
        }
        // The normalized values use the separators of a saved project, so that the project
        // holds what the file will contain.
        const auto normalized = [](const QString &value, const QString &was) {
            const auto saved = kit::Project::savedPathText(value);
            return saved != was ? std::optional(saved) : std::nullopt;
        };
        if (hasTrack) {
            changes.voiceDir =
                normalized(normalizedVoiceDir(voice, m_settings.voiceLocations()), voiceBefore);
        }
        changes.wavtool = normalized(normalizedSynthTool(m_wavtool->text(), utau), values.wavtool);
        changes.resampler =
            normalized(normalizedSynthTool(m_resampler->text(), utau), values.resampler);
        return changes;
    }

    QLineEdit *ProjectPropertiesDialog::nameEdit() const {
        return m_name;
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
