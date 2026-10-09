#include "ProjectPropertiesDialog.h"

#include <functional>
#include <system_error>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
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
#include "EngineTrust.h"

namespace hello::daw {

    namespace fs = std::filesystem;

    namespace {

        constexpr double MinimumTempo = 10;
        constexpr double MaximumTempo = 512;

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

        // Returns the engine value relative to utau if the engine is inside utau, or else as an
        // absolute path. A value that does not name an existing engine is returned unchanged.
        QString normalizedEngine(const QString &value, const fs::path &utau) {
            if (utau.empty() || !EngineTrust::exists(value, utau)) {
                return value;
            }
            std::error_code rootError;
            std::error_code engineError;
            const auto root = fs::weakly_canonical(utau, rootError);
            const auto engine =
                fs::weakly_canonical(EngineTrust::resolved(value, utau), engineError);
            if (rootError || engineError) {
                return value;
            }
            const auto relative = engine.lexically_relative(root);
            const bool inside = !relative.empty() && *relative.begin() != u"..";
            return textOf(inside ? relative : engine);
        }

        // Returns whether two voiceDir values resolve to the same directory
        bool sameVoiceDirectory(const QString &first, const QString &second,
                                const kit::VoiceLocations &locations) {
            kit::Track firstTrack;
            firstTrack.voiceDir = first;
            kit::Track secondTrack;
            secondTrack.voiceDir = second;
            const auto firstPath = firstTrack.voiceDirectory(locations);
            const auto secondPath = secondTrack.voiceDirectory(locations);
            if (firstPath.empty() || secondPath.empty()) {
                return false;
            }
            std::error_code firstError;
            std::error_code secondError;
            const auto firstCanonical = fs::weakly_canonical(firstPath, firstError);
            const auto secondCanonical = fs::weakly_canonical(secondPath, secondError);
            return !firstError && !secondError && firstCanonical == secondCanonical;
        }

    }

    ProjectPropertiesDialog::ProjectPropertiesDialog(const kit::Project &project,
                                                     AppSettings &settings, QWidget *parent)
        : QDialog(parent), m_project(project), m_settings(settings) {
        setWindowTitle(tr("Project Properties"));
        const auto &values = project.settings;
        const auto warningIcon = style()->standardIcon(QStyle::SP_MessageBoxWarning);

        m_name = new QLineEdit(values.name);
        m_tempo = new QDoubleSpinBox();
        m_tempo->setDecimals(2);
        m_tempo->setRange(MinimumTempo, MaximumTempo);
        m_tempo->setValue(values.tempo);
        connect(m_tempo, &QDoubleSpinBox::valueChanged, this, [this] { m_tempoEdited = true; });
        m_flags = new QLineEdit(values.flags);
        m_outputFile = new QLineEdit(QDir::toNativeSeparators(values.outputFile));

        // The folders in the voice folders are listed by name, and each item holds the %VOICE%
        // value of its folder. A name in a voice folder of higher priority hides the same name
        // in a voice folder of lower priority, as the prefix resolves.
        m_voiceDir = new QComboBox();
        m_voiceDir->setEditable(true);
        m_voiceDir->setInsertPolicy(QComboBox::NoInsert);
        m_voiceDirInvalid = addInvalidMark(m_voiceDir->lineEdit(), warningIcon);
        for (const auto &root : m_settings.voiceLocations().voiceFolders) {
            const auto folders =
                QDir(textOf(root)).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const auto &folder : folders) {
                const auto value = kit::Track::voicePrefix.toString() + folder.fileName();
                if (m_voiceDir->findData(value) < 0) {
                    m_voiceDir->addItem(folder.fileName(), value);
                }
            }
        }
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

        // A relative engine path is relative to the UTAU directory, which is therefore where the
        // file dialog starts.
        const auto engineBrowser = [this](QLineEdit *edit, const QString &title) {
            return [this, edit, title] {
                const auto start = EngineTrust::resolved(edit->text(), m_settings.utauDirectory());
                const auto chosen = QFileDialog::getOpenFileName(this, title, textOf(start));
                if (!chosen.isEmpty()) {
                    edit->setText(QDir::toNativeSeparators(chosen));
                }
            };
        };

        auto reset = new QPushButton(tr("Reset to settings defaults"));
        reset->setObjectName(QStringLiteral("resetProjectEngines"));
        connect(reset, &QPushButton::clicked, this, [this] {
            m_wavtool->setText(QDir::toNativeSeparators(m_settings.wavtool()));
            m_resampler->setText(QDir::toNativeSeparators(m_settings.resampler()));
        });
        auto trust = new QPushButton(tr("Trust project engines"));
        connect(trust, &QPushButton::clicked, this, &ProjectPropertiesDialog::trustEngines);

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
        form->addRow(tr("&Tempo:"), m_tempo);
        form->addRow(tr("&Flags:"), m_flags);
        form->addRow(tr("&Voice folder:"),
                     withBrowse(m_voiceDir, this, [this] { browseVoiceDir(); }));
        form->addRow(tr("&Output file:"), withBrowse(m_outputFile, this, [this] {
                         const auto chosen = QFileDialog::getSaveFileName(
                             this, tr("Choose Output File"), m_outputFile->text(),
                             tr("Wave files (*.wav)"));
                         if (!chosen.isEmpty()) {
                             m_outputFile->setText(QDir::toNativeSeparators(chosen));
                         }
                     }));
        form->addRow(tr("Wav&tool (Tool1):"),
                     withBrowse(m_wavtool, this, engineBrowser(m_wavtool, tr("Choose Wavtool"))));
        form->addRow(
            tr("&Resampler (Tool2):"),
            withBrowse(m_resampler, this, engineBrowser(m_resampler, tr("Choose Resampler"))));
        form->addRow(reset);
        form->addRow(trust);
        form->addRow(m_wavtoolTrust);
        form->addRow(m_resamplerTrust);
        form->addRow(m_untrustedNote);
        form->addRow(m_mode2);

        connect(m_voiceDir->lineEdit(), &QLineEdit::textChanged, this,
                &ProjectPropertiesDialog::checkPaths);
        for (const auto edit : {m_wavtool, m_resampler}) {
            connect(edit, &QLineEdit::textChanged, this, &ProjectPropertiesDialog::checkPaths);
            connect(edit, &QLineEdit::textChanged, this, &ProjectPropertiesDialog::updateTrust);
        }
        checkPaths();
        updateTrust();

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this,
                &ProjectPropertiesDialog::acceptIfValid);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
        resize(560, 0);
    }

    ProjectPropertiesDialog::~ProjectPropertiesDialog() = default;

    void ProjectPropertiesDialog::showVoiceDir(const QString &voiceDir) {
        // A value that resolves into a voice folder is shown by its name in that folder, as an
        // item that holds the %VOICE% value. Any other value is shown as written.
        const auto locations = m_settings.voiceLocations();
        auto value = voiceDir;
        auto display = voiceDir;
        kit::Track track;
        track.voiceDir = voiceDir;
        if (const auto directory = track.voiceDirectory(locations); !directory.empty()) {
            value = kit::Track::voiceDirOf(directory, locations);
            if (value.startsWith(kit::Track::voicePrefix)) {
                display = value.mid(kit::Track::voicePrefix.size());
            }
        }
        auto index = m_voiceDir->findData(value);
        if (index < 0 && value.startsWith(kit::Track::voicePrefix) && value != display) {
            m_voiceDir->addItem(display, value);
            index = m_voiceDir->count() - 1;
        }
        m_voiceDir->setCurrentIndex(index);
        m_voiceDir->setEditText(index >= 0 ? m_voiceDir->itemText(index) : display);
    }

    QString ProjectPropertiesDialog::voiceDirText() const {
        // The name of an item stands for its %VOICE% value. Any other text is the value itself,
        // so that a relative path is relative to the relativeBase of the voice locations, as in
        // the project file.
        const auto text = m_voiceDir->lineEdit()->text();
        for (int index = 0; index < m_voiceDir->count(); ++index) {
            if (text == m_voiceDir->itemText(index)) {
                return m_voiceDir->itemData(index).toString();
            }
        }
        return text;
    }

    fs::path ProjectPropertiesDialog::voiceDirectory() const {
        kit::Track track;
        track.voiceDir = voiceDirText();
        return track.voiceDirectory(m_settings.voiceLocations());
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
        const bool wavtool = EngineTrust::exists(m_wavtool->text(), utau);
        const bool resampler = EngineTrust::exists(m_resampler->text(), utau);
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
        // One complete sentence for each engine and state, so that a translation is not
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
            if (EngineTrust::samePath(value, own, utau)) {
                label->setText(colored(texts.same, m_trustedColor, true));
            } else if (EngineTrust::samePath(value, other, utau)) {
                label->setText(colored(texts.swapped, m_trustedColor, true));
            } else if (EngineTrust::isTrusted(m_settings, value, utau)) {
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
            colored(tr("Warning: project engines are untrusted. Playback does not render until "
                       "the required project engines are trusted."),
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

    void ProjectPropertiesDialog::trustEngines() {
        // An engine that does not exist is marked invalid and is not asked about.
        const auto utau = m_settings.utauDirectory();
        QStringList values;
        for (const auto edit : {m_wavtool, m_resampler}) {
            if (EngineTrust::exists(edit->text(), utau)) {
                values.push_back(edit->text());
            }
        }
        EngineTrust::ask(this, m_settings, values, utau);
        updateTrust();
    }

    void ProjectPropertiesDialog::acceptIfValid() {
        if (checkPaths()) {
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
        kit::ProjectPropertyChanges changes;
        changes.name = text(m_name, values.name);
        if (m_tempoEdited && m_tempo->value() != values.tempo) {
            changes.tempo = m_tempo->value();
        }
        changes.flags = text(m_flags, values.flags);
        changes.outputFile = pathText(m_outputFile->text(), values.outputFile);
        if (!m_project.tracks.isEmpty()) {
            const auto &before = m_project.tracks.first().voiceDir;
            const auto voice = voiceDirText();
            if (voice != before &&
                !sameVoiceDirectory(before, voice, m_settings.voiceLocations())) {
                changes.voiceDir = voice;
            }
        }
        changes.wavtool = pathText(normalizedEngine(m_wavtool->text(), utau), values.wavtool);
        changes.resampler = pathText(normalizedEngine(m_resampler->text(), utau), values.resampler);
        if (m_mode2->isChecked() != values.mode2) {
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
