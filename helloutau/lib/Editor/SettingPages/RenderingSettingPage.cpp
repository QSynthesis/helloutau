#include "RenderingSettingPage.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>

#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
#include <QtGui/QIntValidator>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

#include "AppSettings.h"
#include "EditorSettingPageIds.h"
#include "SettingPageWidgets_p.h"

namespace hello::daw {

    namespace {

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
        const int current = m_settings.rendering().threadCount();
        if (const int index = m_threads->findData(current); index >= 0) {
            m_threads->setCurrentIndex(index);
        } else {
            m_threads->setEditText(QString::number(current));
        }
        playbackForm->addRow(tr("Rendering &threads:"), m_threads);
        const auto updateThreads = [this] {
            m_threads->setEnabled(m_playbackMode->currentData().toInt() !=
                                  RenderingSettings::Prerender);
        };
        updateThreads();

        layout->addWidget(playbackGroup);
        auto logGroup = new QGroupBox(tr("Render Log"), widget);
        auto logForm = new QFormLayout(logGroup);
        m_renderLogMode = new QComboBox();
        m_renderLogMode->addItem(tr("Keep the latest run only"), false);
        m_renderLogMode->addItem(tr("Accumulate runs"), true);
        m_renderLogMode->setCurrentIndex(
            m_renderLogMode->findData(m_settings.rendering().isLogAccumulated()));
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
        const int logLimit = m_settings.rendering().logLimit();
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
