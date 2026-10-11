#include "UpdateSettingPage.h"

#include <algorithm>

#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "AppSettings.h"
#include "SettingPageWidgets_p.h"
#include "UpdateChecker.h"

namespace hello::daw {

    UpdateSettingPage::UpdateSettingPage(AppSettings &settings, UpdateChecker &checker,
                                         QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_settings(settings), m_checker(checker) {
        setTitle(tr("Updates"));
        setDescription(tr("The check for newer versions of the application."));
        setKeywords({QStringLiteral("Updates"), QStringLiteral("Check for Updates")});
    }

    QWidget *UpdateSettingPage::createWidget() {
        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);
        auto group = new QGroupBox(tr("Updates"), widget);
        auto form = new QFormLayout(group);
        m_mode = new QComboBox(widget);
        m_mode->addItem(tr("Manual"), UpdateSettings::Manual);
        m_mode->addItem(tr("Check and notify"), UpdateSettings::Notified);
        m_mode->addItem(tr("Automatic"), UpdateSettings::Automatic);
        m_mode->setCurrentIndex(std::max(0, m_mode->findData(m_settings.updates().mode())));
        form->addRow(tr("Update &mode:"), m_mode);
        form->addRow(note(tr("Manual: updates are checked only from Help > Check for Updates.\n"
                             "Check and notify: updates are checked at startup, at most once a "
                             "day, and a new version is offered.\n"
                             "Automatic: updates are checked at startup as well, and a new "
                             "version is downloaded in the background and installed when the "
                             "application quits.")));
        m_source = new QComboBox(widget);
        // The names of the sources are those of the update configuration, untranslated.
        m_source->addItem(tr("Automatic"), QString());
        if (const auto &configuration = m_checker.configuration()) {
            for (const auto &source : configuration->sources) {
                m_source->addItem(source.name, source.name);
            }
        }
        m_source->setCurrentIndex(std::max(0, m_source->findData(storedSource())));
        form->addRow(tr("Update &source:"), m_source);
        form->addRow(note(tr("Automatic selects a source by the time zone of the system.")));
        auto checkNow = new QPushButton(tr("Check &Now"), widget);
        m_lastCheck = note(QString());
        showLastCheck();
        auto checkRow = new QHBoxLayout();
        checkRow->addWidget(checkNow);
        checkRow->addWidget(m_lastCheck, 1);
        form->addRow(checkRow);
        connect(checkNow, &QPushButton::clicked, this, [this, checkNow] {
            m_checker.checkNow(checkNow->window(), m_source->currentData().toString());
        });
        // The label is the context, because the page outlives its widget.
        connect(&m_checker, &UpdateChecker::finished, m_lastCheck, [this] { showLastCheck(); });
        connect(m_mode, &QComboBox::currentIndexChanged, this, &SettingPage::modifiedChanged);
        connect(m_source, &QComboBox::currentIndexChanged, this, &SettingPage::modifiedChanged);
        layout->addWidget(group);
        layout->addStretch();
        return widget;
    }

    bool UpdateSettingPage::isModified() const {
        if (!m_mode) {
            return false;
        }
        return m_mode->currentData().toInt() != m_settings.updates().mode() ||
               m_source->currentData().toString() != storedSource();
    }

    bool UpdateSettingPage::apply(QString *error) {
        Q_UNUSED(error);
        m_settings.updates().setMode(UpdateSettings::Mode(m_mode->currentData().toInt()));
        m_settings.updates().setSource(m_source->currentData().toString());
        Q_EMIT modifiedChanged();
        return true;
    }

    QComboBox *UpdateSettingPage::modeBox() const {
        return m_mode;
    }

    QComboBox *UpdateSettingPage::sourceBox() const {
        return m_source;
    }

    QString UpdateSettingPage::storedSource() const {
        const auto source = m_settings.updates().source();
        const auto &configuration = m_checker.configuration();
        return configuration && configuration->hasSource(source) ? source : QString();
    }

    void UpdateSettingPage::showLastCheck() {
        const auto last = m_settings.updates().lastCheck();
        m_lastCheck->setText(
            tr("Current version: %1. Last checked: %2.")
                .arg(QCoreApplication::applicationVersion(),
                     last.isValid() ? QLocale().toString(last.toLocalTime(), QLocale::ShortFormat)
                                    : tr("never")));
    }

}
