#include "EditorSettingPages_p.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>

#include <QtCore/QDir>
#include <QtGui/QIntValidator>
#include <QtCore/QRegularExpression>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "AppSettings.h"
#include "EditorSettingPageIds.h"
#include "ExportUstDialog.h"
#include "Restarter.h"
#include "Translations.h"

#include <helloutau/Audio/AudioEngine.h>
#include <helloutau/Audio/AudioOutput.h>
#include <helloutau/Audio/SineWaveSource.h>

namespace hello::daw {

    namespace {

        QString pathText(const QLineEdit *edit) {
            return QDir::fromNativeSeparators(edit->text().trimmed());
        }

        // A row with a line edit and a button that browses for a directory or a file
        QLineEdit *addPathRow(QFormLayout *form, QWidget *owner, const QString &label,
                              const QString &text, bool directory) {
            auto edit = new QLineEdit(QDir::toNativeSeparators(text));
            auto button = new QPushButton(SettingPage::tr("Browse..."));
            QObject::connect(button, &QPushButton::clicked, owner, [owner, edit, directory] {
                const auto chosen =
                    directory
                        ? QFileDialog::getExistingDirectory(owner, SettingPage::tr("Choose Folder"),
                                                            edit->text())
                        : QFileDialog::getOpenFileName(owner, SettingPage::tr("Choose Program"),
                                                       edit->text());
                if (!chosen.isEmpty()) {
                    edit->setText(QDir::toNativeSeparators(chosen));
                }
            });
            auto layout = new QHBoxLayout();
            layout->addWidget(edit, 1);
            layout->addWidget(button);
            form->addRow(label, layout);
            return edit;
        }

        QLabel *note(const QString &text) {
            auto label = new QLabel(text);
            label->setWordWrap(true);
            return label;
        }

        std::optional<int> renderLogBytes(const QString &text) {
            static const QRegularExpression pattern(
                QStringLiteral(R"(^\s*(\d+(?:\.\d+)?)\s*(B|KiB|MiB|GiB)?\s*$)"),
                QRegularExpression::CaseInsensitiveOption);
            const auto match = pattern.match(text);
            if (!match.hasMatch()) {
                return std::nullopt;
            }
            bool ok = false;
            const double number = match.captured(1).toDouble(&ok);
            if (!ok) {
                return std::nullopt;
            }
            const auto suffix = match.captured(2).toLower();
            const double multiplier = suffix == QLatin1String("kib")   ? 1024.0
                                      : suffix == QLatin1String("mib") ? 1024.0 * 1024
                                      : suffix == QLatin1String("gib") ? 1024.0 * 1024 * 1024
                                                                       : 1.0;
            const double bytes = number * multiplier;
            if (bytes < 1024 || bytes > 1024.0 * 1024 * 1024 || bytes != std::floor(bytes)) {
                return std::nullopt;
            }
            return int(bytes);
        }

    }

    SystemSettingsPage::SystemSettingsPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(EditorSettingPageIds::systemSettings), parent),
          m_settings(settings) {
        setTitle(tr("System Settings"));
        setDescription(tr("The language of the interface."));
        setKeywords({QStringLiteral("System Settings"), QStringLiteral("Language")});
    }

    QWidget *SystemSettingsPage::createWidget() {
        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);
        auto interfaceGroup = new QGroupBox(tr("Interface"), widget);
        auto interfaceForm = new QFormLayout(interfaceGroup);
        m_language = new QComboBox(widget);
        m_language->setObjectName(QStringLiteral("language"));
        for (const auto &[language, name] : Translations::languages()) {
            m_language->addItem(name, language);
        }
        m_language->setCurrentIndex(std::max(0, m_language->findData(m_settings.language())));
        interfaceForm->addRow(tr("&Language:"), m_language);
        interfaceForm->addRow(note(tr("Takes effect after a restart.")));
        connect(m_language, &QComboBox::currentIndexChanged, this, &SettingPage::modifiedChanged);
        layout->addWidget(interfaceGroup);
        layout->addStretch();
        return widget;
    }

    bool SystemSettingsPage::isModified() const {
        if (!m_language) {
            return false;
        }
        return m_language->currentData().toString() != m_settings.language();
    }

    bool SystemSettingsPage::apply(QString *error) {
        Q_UNUSED(error);
        const auto language = m_language->currentData().toString();
        if (language != m_settings.language()) {
            m_settings.setLanguage(language);
            Restarter::markNeeded();
        }
        Q_EMIT modifiedChanged();
        return true;
    }

    QComboBox *SystemSettingsPage::languageBox() const {
        return m_language;
    }

    UtauSettingPage::UtauSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(EditorSettingPageIds::utau), parent), m_settings(settings) {
        setTitle(QStringLiteral("UTAU"));
        setDescription(
            tr("Where UTAU is, and how a project finds its voice bank and synth tools."));
        setKeywords({QStringLiteral("UTAU"), QStringLiteral("voice"), QStringLiteral("tools")});
    }

    QWidget *UtauSettingPage::createWidget() {
        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);
        auto utauGroup = new QGroupBox(tr("Folder"), widget);
        auto utauForm = new QFormLayout(utauGroup);
        m_utauDirectory =
            addPathRow(utauForm, widget, tr("&UTAU folder:"),
                       QString::fromStdU16String(m_settings.utauDirectory().u16string()), true);
        utauForm->addRow(
            note(tr("The folder that contains utau.exe. A synth tool that a project "
                    "names by a relative path is resolved against it, and its voice and "
                    "plugins folders are used as well.")));
        m_relativeVoiceDirInUtau = new QCheckBox(
            tr("Resolve a &relative voice bank path against the UTAU folder"), widget);
        m_relativeVoiceDirInUtau->setChecked(m_settings.isRelativeVoiceDirInUtau());
        utauForm->addRow(m_relativeVoiceDirInUtau);
        utauForm->addRow(note(tr("UTAU resolves a relative voice bank path against its own folder. "
                                 "If this option is cleared, or if the UTAU folder does not exist, "
                                 "the path is resolved against the folder of HelloUtau.")));
        m_voiceFolders = note(QString());
        utauForm->addRow(m_voiceFolders);
        showVoiceFolders();
        connect(m_utauDirectory, &QLineEdit::textChanged, this, &SettingPage::modifiedChanged);
        connect(m_utauDirectory, &QLineEdit::textChanged, this, &UtauSettingPage::showVoiceFolders);
        connect(m_relativeVoiceDirInUtau, &QCheckBox::toggled, this, &SettingPage::modifiedChanged);
        layout->addWidget(utauGroup);
        layout->addStretch();
        return widget;
    }

    bool UtauSettingPage::isModified() const {
        if (!m_utauDirectory) {
            return false;
        }
        return pathText(m_utauDirectory) != QDir::fromNativeSeparators(QString::fromStdU16String(
                                                m_settings.utauDirectory().u16string())) ||
               m_relativeVoiceDirInUtau->isChecked() != m_settings.isRelativeVoiceDirInUtau();
    }

    void UtauSettingPage::showVoiceFolders() {
        // As voiceLocations() lists them for the UTAU folder being edited
        QStringList lines{tr("The voice folders that %VOICE% denotes, in decreasing priority:")};
        lines.push_back(QDir::toNativeSeparators(
            QString::fromStdU16String(m_settings.voiceFolder().u16string())));
        const auto utau = pathText(m_utauDirectory);
        if (!utau.isEmpty()) {
            lines.push_back(QDir::toNativeSeparators(utau + QStringLiteral("/voice")));
        }
        m_voiceFolders->setText(lines.join(QLatin1Char('\n')));
    }

    bool UtauSettingPage::apply(QString *error) {
        Q_UNUSED(error);
        m_settings.setUtauDirectory(
            std::filesystem::path(pathText(m_utauDirectory).toStdU16String()));
        m_settings.setRelativeVoiceDirInUtau(m_relativeVoiceDirInUtau->isChecked());
        Q_EMIT modifiedChanged();
        return true;
    }

    QLineEdit *UtauSettingPage::utauDirectoryEdit() const {
        return m_utauDirectory;
    }

    QCheckBox *UtauSettingPage::relativeVoiceDirInUtauBox() const {
        return m_relativeVoiceDirInUtau;
    }

    EditorSettingPage::EditorSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(EditorSettingPageIds::editor), parent), m_settings(settings) {
        setTitle(tr("Editor"));
        setDescription(tr("How projects are edited and exported."));
        setKeywords({QStringLiteral("Editor"), QStringLiteral("UST")});
    }

    QWidget *EditorSettingPage::createWidget() {
        auto widget = new QWidget();
        auto form = new QFormLayout(widget);
        m_ustExportCharset = new QComboBox();
        m_ustExportCharset->addItems(ExportUstDialog::charsets());
        const int index = m_ustExportCharset->findText(m_settings.ustExportCharset());
        m_ustExportCharset->setCurrentIndex(index >= 0 ? index : 0);
        form->addRow(tr("Default UST &export encoding:"), m_ustExportCharset);
        connect(m_ustExportCharset, &QComboBox::currentTextChanged, this,
                &SettingPage::modifiedChanged);
        return widget;
    }

    bool EditorSettingPage::isModified() const {
        return m_ustExportCharset &&
               m_ustExportCharset->currentText() != m_settings.ustExportCharset();
    }

    bool EditorSettingPage::apply(QString *error) {
        Q_UNUSED(error);
        m_settings.setUstExportCharset(m_ustExportCharset->currentText());
        Q_EMIT modifiedChanged();
        return true;
    }

    QComboBox *EditorSettingPage::ustExportCharsetBox() const {
        return m_ustExportCharset;
    }

    AudioSettingPage::AudioSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(EditorSettingPageIds::audio), parent), m_settings(settings) {
        setTitle(tr("Audio"));
        setDescription(tr("The output device used for playback."));
        setKeywords({QStringLiteral("Audio"), QStringLiteral("Output"), QStringLiteral("Device")});
    }

    QWidget *AudioSettingPage::createWidget() {
        auto widget = new QWidget();
        auto form = new QFormLayout(widget);
        m_output = new QComboBox(widget);
        m_output->addItem(tr("System default"), QByteArray());
        const auto selected = m_settings.audioOutputDevice();
        int selectedIndex = 0;
        int index = 1;
        for (const auto &id : AudioEngine::deviceIds()) {
            m_output->addItem(AudioEngine::deviceDescription(id), id);
            if (id == selected) {
                selectedIndex = index;
            }
            ++index;
        }
        m_output->setCurrentIndex(selectedIndex);
        form->addRow(tr("&Output device:"), m_output);
        auto test = new QPushButton(tr("Test"), widget);
        form->addRow({}, test);
        form->addRow(note(tr("The test plays a short sine wave on the selected device.")));
        connect(m_output, &QComboBox::currentIndexChanged, this, &SettingPage::modifiedChanged);
        // The tone plays on the device selected in the box, which need not be applied yet.
        connect(test, &QPushButton::clicked, widget, [this, widget] {
            const auto id = m_output->currentData().toByteArray();
            const int rate = AudioEngine::instance()->sampleRate(id);
            QString error = tr("There is no audio output device.");
            if (rate > 0) {
                auto output = new AudioOutput(widget);
                connect(output, &AudioOutput::finished, output, &QObject::deleteLater);
                if (output->start(std::make_shared<SineWaveSource>(rate, 440.0, 0.5), id, rate,
                                  &error)) {
                    return;
                }
                delete output;
            }
            QMessageBox::warning(widget, tr("Test"), error);
        });
        return widget;
    }

    bool AudioSettingPage::isModified() const {
        return m_output && m_output->currentData().toByteArray() != m_settings.audioOutputDevice();
    }

    bool AudioSettingPage::apply(QString *error) {
        Q_UNUSED(error);
        const auto id = m_output->currentData().toByteArray();
        m_settings.setAudioOutputDevice(id);
        AudioEngine::instance()->setDeviceId(id);
        Q_EMIT modifiedChanged();
        return true;
    }

    RenderingSettingPage::RenderingSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(EditorSettingPageIds::rendering), parent),
          m_settings(settings) {
        setTitle(tr("Audio Rendering"));
        setDescription(
            tr("The synth tools used for audio rendering, and how playback renders audio."));
        setKeywords({QStringLiteral("Audio Rendering"), QStringLiteral("resampler"),
                     QStringLiteral("wavtool"), QStringLiteral("playback")});
    }

    QWidget *RenderingSettingPage::createWidget() {
        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);
        auto synthToolsGroup = new QGroupBox(tr("Synth tools"), widget);
        auto synthToolsForm = new QFormLayout(synthToolsGroup);
        m_wavtool =
            addPathRow(synthToolsForm, widget, tr("&Wavtool:"), m_settings.wavtool(), false);
        m_resampler =
            addPathRow(synthToolsForm, widget, tr("&Resampler:"), m_settings.resampler(), false);
        synthToolsForm->addRow(
            note(tr("Project Properties resets the synth tools of a project to these. "
                    "The voice bank editor previews entries with this resampler.")));
        layout->addWidget(synthToolsGroup);

        auto playbackGroup = new QGroupBox(tr("Playback"), widget);
        auto playbackForm = new QFormLayout(playbackGroup);
        m_playbackMode = new QComboBox();
        m_playbackMode->addItem(tr("Classic prerender, in an external console as UTAU does"),
                                AppSettings::Prerender);
        m_playbackMode->addItem(tr("Threaded prerender, rendered by several threads"),
                                AppSettings::ThreadedPrerender);
        m_playbackMode->addItem(tr("Realtime, rendered in the background from the playhead"),
                                AppSettings::Realtime);
        m_playbackMode->setCurrentIndex(m_playbackMode->findData(m_settings.playbackMode()));
        playbackForm->addRow(tr("&Playback:"), m_playbackMode);
        playbackForm->addRow(
            note(tr("Realtime playback joins the notes by the rules of the project "
                    "wavtool without running the wavtool process. Rendering a "
                    "whole track uses an external console in the classic mode and "
                    "several threads otherwise.")));

        // Zero stands for one thread per hardware thread. The list offers the powers of two below
        // the number of hardware threads and that number, and any other count can be typed in.
        const int hardware = int(std::max(1u, std::thread::hardware_concurrency()));
        m_threads = new QComboBox();
        m_threads->setObjectName(QStringLiteral("threads"));
        m_threads->setEditable(true);
        m_threads->setInsertPolicy(QComboBox::NoInsert);
        m_threads->setValidator(new QIntValidator(1, std::numeric_limits<int>::max(), m_threads));
        m_threads->addItem(tr("Automatic (%1)").arg(hardware), 0);
        for (int count = 1; count < hardware; count *= 2) {
            m_threads->addItem(QString::number(count), count);
        }
        m_threads->addItem(QString::number(hardware), hardware);
        const int current = m_settings.renderThreadCount();
        if (const int index = m_threads->findData(current); index >= 0) {
            m_threads->setCurrentIndex(index);
        } else {
            m_threads->setEditText(QString::number(current));
        }
        playbackForm->addRow(tr("Rendering &threads:"), m_threads);
        const auto updateThreads = [this] {
            m_threads->setEnabled(m_playbackMode->currentData().toInt() != AppSettings::Prerender);
        };
        updateThreads();

        layout->addWidget(playbackGroup);
        auto logGroup = new QGroupBox(tr("Render Log"), widget);
        auto logForm = new QFormLayout(logGroup);
        m_renderLogMode = new QComboBox();
        m_renderLogMode->addItem(tr("Keep the latest run only"), false);
        m_renderLogMode->addItem(tr("Accumulate runs"), true);
        m_renderLogMode->setCurrentIndex(
            m_renderLogMode->findData(m_settings.isRenderLogAccumulated()));
        logForm->addRow(tr("Render &log:"), m_renderLogMode);

        m_renderLogLimit = new QComboBox();
        m_renderLogLimit->setEditable(true);
        m_renderLogLimit->setInsertPolicy(QComboBox::InsertAtBottom);
        const QList<QPair<QString, int>> limits{
            {QStringLiteral("256 KiB"), 256 * 1024      },
            {QStringLiteral("1 MiB"),   1024 * 1024     },
            {QStringLiteral("4 MiB"),   4 * 1024 * 1024 },
            {QStringLiteral("16 MiB"),  16 * 1024 * 1024}
        };
        for (const auto &[label, bytes] : limits) {
            m_renderLogLimit->addItem(label, bytes);
        }
        const int logLimit = m_settings.renderLogLimit();
        if (const int index = m_renderLogLimit->findData(logLimit); index >= 0) {
            m_renderLogLimit->setCurrentIndex(index);
        } else {
            m_renderLogLimit->setEditText(QString::number(logLimit));
        }
        logForm->addRow(tr("Render log &size:"), m_renderLogLimit);
        logForm->addRow(note(tr("Captured output is shared by realtime and threaded rendering. "
                                "Enter a number with B, KiB, MiB or GiB, or choose a preset.")));
        layout->addWidget(logGroup);
        layout->addStretch();

        connect(m_resampler, &QLineEdit::textChanged, this, &SettingPage::modifiedChanged);
        connect(m_wavtool, &QLineEdit::textChanged, this, &SettingPage::modifiedChanged);
        connect(m_playbackMode, &QComboBox::currentIndexChanged, this, updateThreads);
        connect(m_playbackMode, &QComboBox::currentIndexChanged, this,
                &SettingPage::modifiedChanged);
        connect(m_threads, &QComboBox::currentTextChanged, this, &SettingPage::modifiedChanged);
        connect(m_renderLogMode, &QComboBox::currentIndexChanged, this,
                &SettingPage::modifiedChanged);
        connect(m_renderLogLimit, &QComboBox::currentTextChanged, this,
                &SettingPage::modifiedChanged);
        connect(m_renderLogLimit, &QComboBox::editTextChanged, this, &SettingPage::modifiedChanged);
        return widget;
    }

    std::optional<int> RenderingSettingPage::threadCount() const {
        const auto text = m_threads->currentText();
        if (const int index = m_threads->findText(text); index >= 0) {
            return m_threads->itemData(index).toInt();
        }
        bool number = false;
        const int count = text.trimmed().toInt(&number);
        if (!number || count < 1) {
            return std::nullopt;
        }
        return count;
    }

    std::optional<int> RenderingSettingPage::renderLogLimit() const {
        const int index =
            m_renderLogLimit->findText(m_renderLogLimit->currentText(), Qt::MatchExactly);
        if (index >= 0 && m_renderLogLimit->itemData(index).isValid()) {
            return m_renderLogLimit->itemData(index).toInt();
        }
        return renderLogBytes(m_renderLogLimit->currentText());
    }

    bool RenderingSettingPage::isModified() const {
        if (!m_resampler || !m_wavtool || !m_playbackMode || !m_threads || !m_renderLogMode ||
            !m_renderLogLimit) {
            return false;
        }
        return pathText(m_resampler) != QDir::fromNativeSeparators(m_settings.resampler()) ||
               pathText(m_wavtool) != QDir::fromNativeSeparators(m_settings.wavtool()) ||
               m_playbackMode->currentData().toInt() != m_settings.playbackMode() ||
               threadCount() != m_settings.renderThreadCount() ||
               m_renderLogMode->currentData().toBool() != m_settings.isRenderLogAccumulated() ||
               renderLogLimit() != m_settings.renderLogLimit();
    }

    bool RenderingSettingPage::apply(QString *error) {
        const auto threads = threadCount();
        if (!threads) {
            if (error) {
                *error = tr("The number of rendering threads is a positive whole number, or "
                            "Automatic.");
            }
            return false;
        }
        const auto logLimit = renderLogLimit();
        if (!logLimit) {
            if (error) {
                *error = tr("The render log size must be between 1024 bytes and 1 GiB.");
            }
            return false;
        }
        m_settings.setResampler(pathText(m_resampler));
        m_settings.setWavtool(pathText(m_wavtool));
        m_settings.setPlaybackMode(
            AppSettings::PlaybackMode(m_playbackMode->currentData().toInt()));
        m_settings.setRenderThreadCount(*threads);
        m_settings.setRenderLogAccumulated(m_renderLogMode->currentData().toBool());
        m_settings.setRenderLogLimit(*logLimit);
        Q_EMIT modifiedChanged();
        return true;
    }

    QLineEdit *RenderingSettingPage::resamplerEdit() const {
        return m_resampler;
    }

    QLineEdit *RenderingSettingPage::wavtoolEdit() const {
        return m_wavtool;
    }

    QComboBox *RenderingSettingPage::playbackModeBox() const {
        return m_playbackMode;
    }

    QComboBox *RenderingSettingPage::threadCountBox() const {
        return m_threads;
    }

    void addEditorSettingPages(SettingCatalog *catalog, AppSettings &settings) {
        // Appearance & Behavior is a category, which the dialog shows as the links to its pages.
        auto appearance =
            new SettingPage(QLatin1String(EditorSettingPageIds::appearanceAndBehavior));
        appearance->setTitle(SettingPage::tr("Appearance & Behavior"));
        appearance->setKeywords({QStringLiteral("Appearance & Behavior")});
        appearance->addPage(new SystemSettingsPage(settings));
        catalog->addPage(appearance);
        catalog->addPage(new EditorSettingPage(settings));
        catalog->addPage(new UtauSettingPage(settings));
        catalog->addPage(new AudioSettingPage(settings));
        catalog->addPage(new RenderingSettingPage(settings));
    }

}
