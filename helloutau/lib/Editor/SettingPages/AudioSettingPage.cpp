#include "AudioSettingPage.h"

#include <algorithm>
#include <memory>

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include <helloutau/Audio/AudioEngine.h>
#include <helloutau/Audio/AudioOutput.h>
#include <helloutau/Audio/SineWaveSource.h>

#include "AppSettings.h"
#include "SettingPageWidgets_p.h"

namespace hello::daw {

    AudioSettingPage::AudioSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_settings(settings) {
        setTitle(tr("Audio"));
        setDescription(tr("The output device used for playback."));
        setKeywords({QStringLiteral("Audio"), QStringLiteral("Output"), QStringLiteral("Device")});
    }

    QWidget *AudioSettingPage::createWidget() {
        auto widget = new QWidget();
        auto form = new QFormLayout(widget);
        m_output = new QComboBox(widget);
        fillDevices(m_settings.audio().outputDevice());
        form->addRow(tr("&Output device:"), m_output);
        auto test = new QPushButton(tr("Test"), widget);
        form->addRow({}, test);
        form->addRow(note(tr("The test plays a short sine wave on the selected device.")));
        connect(m_output, &QComboBox::currentIndexChanged, this, &SettingPage::modifiedChanged);
        // The list follows the devices while the page is open, and keeps the device in the box
        // if it is still present.
        connect(AudioEngine::instance(), &AudioEngine::devicesChanged, widget, [this] {
            // An open list keeps the size of the items it was opened with, so that it would show
            // blank rows or a scroll bar. The user opens the list again.
            m_output->hidePopup();
            fillDevices(m_output->currentData().toByteArray());
            Q_EMIT modifiedChanged();
        });
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

    void AudioSettingPage::fillDevices(const QByteArray &current) {
        const QSignalBlocker blocker(m_output);
        m_output->clear();
        m_output->addItem(tr("System default"), QByteArray());
        for (const auto &id : AudioEngine::deviceIds()) {
            m_output->addItem(AudioEngine::deviceDescription(id), id);
        }
        m_output->setCurrentIndex(std::max(0, m_output->findData(current)));
    }

    bool AudioSettingPage::isModified() const {
        return m_output &&
               m_output->currentData().toByteArray() != m_settings.audio().outputDevice();
    }

    bool AudioSettingPage::apply(QString *error) {
        Q_UNUSED(error);
        const auto id = m_output->currentData().toByteArray();
        m_settings.audio().setOutputDevice(id);
        AudioEngine::instance()->setDeviceId(id);
        Q_EMIT modifiedChanged();
        return true;
    }

}
