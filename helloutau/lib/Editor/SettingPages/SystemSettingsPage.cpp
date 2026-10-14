#include "SystemSettingsPage.h"

#include <algorithm>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Support/ExitLauncher.h>

#include "AppSettings.h"
#include "SettingPageWidgets_p.h"

namespace hello::daw {

    SystemSettingsPage::SystemSettingsPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_settings(settings) {
        setTitle(tr("System Settings"));
        setDescription(tr("The language of the interface and the threads of the general tasks."));
        setKeywords({QStringLiteral("System Settings"), QStringLiteral("Language"),
                     QStringLiteral("Threads")});
    }

    QWidget *SystemSettingsPage::createWidget() {
        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);
        auto interfaceGroup = new QGroupBox(tr("Interface"), widget);
        auto interfaceForm = new QFormLayout(interfaceGroup);
        m_language = new QComboBox(widget);
        m_language->setObjectName(QStringLiteral("language"));
        for (const auto &[language, name] : AppearanceSettings::languages()) {
            m_language->addItem(name, language);
        }
        m_language->setCurrentIndex(
            std::max(0, m_language->findData(m_settings.appearance().language())));
        interfaceForm->addRow(tr("&Language:"), m_language);
        interfaceForm->addRow(note(tr("The change takes effect after a restart.")));
        connect(m_language, &QComboBox::currentIndexChanged, this, &SettingPage::modifiedChanged);
        layout->addWidget(interfaceGroup);

        auto tasksGroup = new QGroupBox(tr("Tasks"), widget);
        auto tasksForm = new QFormLayout(tasksGroup);
        m_taskThreads = createThreadCountBox(m_settings.tasks().threadCount());
        m_taskThreads->setObjectName(QStringLiteral("taskThreads"));
        tasksForm->addRow(tr("Task &threads:"), m_taskThreads);
        tasksForm->addRow(note(tr("The number of threads of the general tasks of the editor, such "
                                  "as reading the audio files of a voice bank before they are "
                                  "processed. The rendering and the frequency maps have threads "
                                  "of their own.")));
        connect(m_taskThreads, &QComboBox::currentTextChanged, this, &SettingPage::modifiedChanged);
        layout->addWidget(tasksGroup);
        layout->addStretch();
        return widget;
    }

    bool SystemSettingsPage::isModified() const {
        if (!m_language || !m_taskThreads) {
            return false;
        }
        return m_language->currentData().toString() != m_settings.appearance().language() ||
               threadCountOf(m_taskThreads) != m_settings.tasks().threadCount();
    }

    bool SystemSettingsPage::apply(QString *error) {
        const auto threads = threadCountOf(m_taskThreads);
        if (!threads) {
            if (error) {
                *error = tr("The number of task threads is a positive whole number, or "
                            "Automatic.");
            }
            return false;
        }
        m_settings.tasks().setThreadCount(*threads);
        const auto language = m_language->currentData().toString();
        if (language != m_settings.appearance().language()) {
            m_settings.appearance().setLanguage(language);
            if (const auto launcher = kit::ExitLauncher::instance()) {
                launcher->requireRestart();
            }
        }
        Q_EMIT modifiedChanged();
        return true;
    }

    QComboBox *SystemSettingsPage::languageBox() const {
        return m_language;
    }

    QComboBox *SystemSettingsPage::taskThreadsBox() const {
        return m_taskThreads;
    }

}
