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
        for (const auto &[language, name] : AppearanceSettings::languages()) {
            m_language->addItem(name, language);
        }
        m_language->setCurrentIndex(
            std::max(0, m_language->findData(m_settings.appearance().language())));
        interfaceForm->addRow(tr("&Language:"), m_language);
        interfaceForm->addRow(note(tr("The change takes effect after a restart.")));
        connect(m_language, &QComboBox::currentIndexChanged, this, &SettingPage::modifiedChanged);
        layout->addWidget(interfaceGroup);
        layout->addStretch();
        return widget;
    }

    bool SystemSettingsPage::isModified() const {
        if (!m_language) {
            return false;
        }
        return m_language->currentData().toString() != m_settings.appearance().language();
    }

    bool SystemSettingsPage::apply(QString *error) {
        Q_UNUSED(error);
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

}
