#include "EditorSettingPages_p.h"

#include <algorithm>
#include <limits>
#include <thread>

#include <QtCore/QDir>
#include <QtGui/QIntValidator>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

#include "AppSettings.h"
#include "EditorSettingPageIds.h"
#include "ExportUstDialog.h"
#include "Restarter.h"
#include "Translations.h"

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

    }

    SystemSettingsPage::SystemSettingsPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(EditorSettingPageIds::systemSettings), parent),
          m_settings(settings) {
        setTitle(tr("System Settings"));
        setDescription(tr("The language of the interface, and where UTAU is."));
        setKeywords({QStringLiteral("System Settings"), QStringLiteral("Language"),
                     QStringLiteral("UTAU")});
    }

    QWidget *SystemSettingsPage::createWidget() {
        auto widget = new QWidget();
        auto form = new QFormLayout(widget);
        m_language = new QComboBox(widget);
        m_language->setObjectName(QStringLiteral("language"));
        for (const auto &[language, name] : Translations::languages()) {
            m_language->addItem(name, language);
        }
        m_language->setCurrentIndex(std::max(0, m_language->findData(m_settings.language())));
        form->addRow(tr("&Language:"), m_language);
        form->addRow(note(tr("Takes effect after a restart.")));
        connect(m_language, &QComboBox::currentIndexChanged, this, &SettingPage::modifiedChanged);
        m_utauDirectory =
            addPathRow(form, widget, tr("&UTAU folder:"),
                       QString::fromStdU16String(m_settings.utauDirectory().u16string()), true);
        form->addRow(note(tr("Resolves the voice banks of projects that name them relative to "
                             "UTAU, such as %VOICE%.")));
        connect(m_utauDirectory, &QLineEdit::textChanged, this, &SettingPage::modifiedChanged);
        return widget;
    }

    bool SystemSettingsPage::isModified() const {
        if (!m_utauDirectory) {
            return false;
        }
        return m_language->currentData().toString() != m_settings.language() ||
               pathText(m_utauDirectory) != QDir::fromNativeSeparators(QString::fromStdU16String(
                                                m_settings.utauDirectory().u16string()));
    }

    bool SystemSettingsPage::apply(QString *error) {
        Q_UNUSED(error);
        const auto language = m_language->currentData().toString();
        if (language != m_settings.language()) {
            m_settings.setLanguage(language);
            Restarter::markNeeded();
        }
        m_settings.setUtauDirectory(
            std::filesystem::path(pathText(m_utauDirectory).toStdU16String()));
        Q_EMIT modifiedChanged();
        return true;
    }

    QLineEdit *SystemSettingsPage::utauDirectoryEdit() const {
        return m_utauDirectory;
    }

    QComboBox *SystemSettingsPage::languageBox() const {
        return m_language;
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
        form->addRow(tr("UST &export encoding:"), m_ustExportCharset);
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
        for (const auto &id : AudioOutput::outputDeviceIds()) {
            m_output->addItem(AudioOutput::outputDeviceDescription(id), id);
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
        connect(test, &QPushButton::clicked, widget, [this, widget] {
            const int rate = AudioOutput::deviceSampleRate();
            if (rate <= 0) {
                return;
            }
            auto output = new AudioOutput(widget);
            output->start(std::make_shared<SineWaveSource>(rate, 440.0, 0.5));
            connect(output, &AudioOutput::finished, output, &QObject::deleteLater);
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
        AudioOutput::setOutputDeviceId(id);
        Q_EMIT modifiedChanged();
        return true;
    }

    RenderingSettingPage::RenderingSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(EditorSettingPageIds::rendering), parent),
          m_settings(settings) {
        setTitle(tr("Audio Rendering"));
        setDescription(tr("The engines used for audio rendering, and how playback renders audio."));
        setKeywords({QStringLiteral("Audio Rendering"), QStringLiteral("resampler"),
                     QStringLiteral("wavtool"), QStringLiteral("playback")});
    }

    QWidget *RenderingSettingPage::createWidget() {
        auto widget = new QWidget();
        auto form = new QFormLayout(widget);
        m_wavtool = addPathRow(form, widget, tr("&Wavtool:"), m_settings.wavtool(), false);
        m_resampler = addPathRow(form, widget, tr("&Resampler:"), m_settings.resampler(), false);
        form->addRow(note(tr("A project renders with these engines, not with those it names.")));

        m_playbackMode = new QComboBox();
        m_playbackMode->addItem(tr("Classic prerender, in an external console as UTAU does"),
                                AppSettings::Prerender);
        m_playbackMode->addItem(tr("Threaded prerender, rendered by several threads"),
                                AppSettings::ThreadedPrerender);
        m_playbackMode->addItem(tr("Realtime, rendered in the background from the playhead"),
                                AppSettings::Realtime);
        m_playbackMode->setCurrentIndex(m_playbackMode->findData(m_settings.playbackMode()));
        form->addRow(tr("&Playback:"), m_playbackMode);
        form->addRow(note(tr("Realtime playback joins the notes by the rules of the project "
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
        form->addRow(tr("Rendering &threads:"), m_threads);
        const auto updateThreads = [this] {
            m_threads->setEnabled(m_playbackMode->currentData().toInt() != AppSettings::Prerender);
        };
        updateThreads();

        connect(m_resampler, &QLineEdit::textChanged, this, &SettingPage::modifiedChanged);
        connect(m_wavtool, &QLineEdit::textChanged, this, &SettingPage::modifiedChanged);
        connect(m_playbackMode, &QComboBox::currentIndexChanged, this, updateThreads);
        connect(m_playbackMode, &QComboBox::currentIndexChanged, this,
                &SettingPage::modifiedChanged);
        connect(m_threads, &QComboBox::currentTextChanged, this, &SettingPage::modifiedChanged);
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

    bool RenderingSettingPage::isModified() const {
        if (!m_resampler || !m_wavtool || !m_playbackMode || !m_threads) {
            return false;
        }
        return pathText(m_resampler) != QDir::fromNativeSeparators(m_settings.resampler()) ||
               pathText(m_wavtool) != QDir::fromNativeSeparators(m_settings.wavtool()) ||
               m_playbackMode->currentData().toInt() != m_settings.playbackMode() ||
               threadCount() != m_settings.renderThreadCount();
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
        m_settings.setResampler(pathText(m_resampler));
        m_settings.setWavtool(pathText(m_wavtool));
        m_settings.setPlaybackMode(
            AppSettings::PlaybackMode(m_playbackMode->currentData().toInt()));
        m_settings.setRenderThreadCount(*threads);
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
        catalog->addPage(new AudioSettingPage(settings));
        catalog->addPage(new RenderingSettingPage(settings));
    }

}
