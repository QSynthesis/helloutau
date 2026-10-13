#include "RenderingSettingPage.h"

#include <QtCore/QDir>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

#include "AppSettings.h"
#include "SettingPageWidgets_p.h"

namespace hello::daw {

    RenderingSettingPage::RenderingSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_settings(settings) {
        setTitle(tr("Audio Rendering"));
        setDescription(
            tr("The synth tools used for audio rendering, and the way playback renders audio."));
        setKeywords({QStringLiteral("Audio Rendering"), QStringLiteral("resampler"),
                     QStringLiteral("wavtool"), QStringLiteral("playback")});
    }

    QWidget *RenderingSettingPage::createWidget() {
        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);
        auto synthToolsGroup = new QGroupBox(tr("Synth tools"), widget);
        auto synthToolsForm = new QFormLayout(synthToolsGroup);
        m_wavtool = addPathRow(synthToolsForm, widget, tr("&Wavtool:"),
                               m_settings.rendering().wavtool(), false);
        m_resampler = addPathRow(synthToolsForm, widget, tr("&Resampler:"),
                                 m_settings.rendering().resampler(), false);
        synthToolsForm->addRow(
            note(tr("Project Properties resets the synth tools of a project to these. "
                    "The voice bank editor previews entries with this resampler.")));
        layout->addWidget(synthToolsGroup);

        auto playbackGroup = new QGroupBox(tr("Playback"), widget);
        auto playbackForm = new QFormLayout(playbackGroup);
        m_playbackMode = new QComboBox();
        m_playbackMode->addItem(tr("Classic prerender, in an external console as UTAU does"),
                                RenderingSettings::Prerender);
        m_playbackMode->addItem(tr("Threaded prerender, rendered by several threads"),
                                RenderingSettings::ThreadedPrerender);
        m_playbackMode->addItem(tr("Realtime, rendered in the background from the playhead"),
                                RenderingSettings::Realtime);
        m_playbackMode->setCurrentIndex(
            m_playbackMode->findData(m_settings.rendering().playbackMode()));
        playbackForm->addRow(tr("&Playback:"), m_playbackMode);
        playbackForm->addRow(
            note(tr("Realtime playback joins the notes by the rules of the project "
                    "wavtool without running the wavtool process. Rendering a "
                    "whole track uses an external console in the classic mode and "
                    "several threads otherwise.")));

        m_threads = createThreadCountBox(m_settings.rendering().threadCount());
        m_threads->setObjectName(QStringLiteral("threads"));
        playbackForm->addRow(tr("Rendering &threads:"), m_threads);
        const auto updateThreads = [this] {
            m_threads->setEnabled(m_playbackMode->currentData().toInt() !=
                                  RenderingSettings::Prerender);
        };
        updateThreads();

        layout->addWidget(playbackGroup);
        auto logGroup = new QGroupBox(tr("Render Log"), widget);
        auto logForm = new QFormLayout(logGroup);
        m_renderLogMode = createLogModeBox(m_settings.rendering().isLogAccumulated());
        logForm->addRow(tr("Render &log:"), m_renderLogMode);
        m_renderLogLimit = createLogLimitBox(m_settings.rendering().logLimit());
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
        return threadCountOf(m_threads);
    }

    std::optional<int> RenderingSettingPage::renderLogLimit() const {
        return logLimitOf(m_renderLogLimit);
    }

    bool RenderingSettingPage::isModified() const {
        if (!m_resampler || !m_wavtool || !m_playbackMode || !m_threads || !m_renderLogMode ||
            !m_renderLogLimit) {
            return false;
        }
        return pathText(m_resampler) !=
                   QDir::fromNativeSeparators(m_settings.rendering().resampler()) ||
               pathText(m_wavtool) !=
                   QDir::fromNativeSeparators(m_settings.rendering().wavtool()) ||
               m_playbackMode->currentData().toInt() != m_settings.rendering().playbackMode() ||
               threadCount() != m_settings.rendering().threadCount() ||
               m_renderLogMode->currentData().toBool() !=
                   m_settings.rendering().isLogAccumulated() ||
               renderLogLimit() != m_settings.rendering().logLimit();
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
        m_settings.rendering().setResampler(pathText(m_resampler));
        m_settings.rendering().setWavtool(pathText(m_wavtool));
        m_settings.rendering().setPlaybackMode(
            RenderingSettings::PlaybackMode(m_playbackMode->currentData().toInt()));
        m_settings.rendering().setThreadCount(*threads);
        m_settings.rendering().setLogAccumulated(m_renderLogMode->currentData().toBool());
        m_settings.rendering().setLogLimit(*logLimit);
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

}
