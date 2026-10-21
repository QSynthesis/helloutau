#include "UpdateSettingPage.h"

#include <algorithm>

#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

#include <helloutau/Widgets/FormLayout.h>

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
        auto form = new FormLayout(group);
        m_mode = new QComboBox(widget);
        m_mode->addItem(tr("Manual"), UpdateSettings::Manual);
        m_mode->addItem(tr("Check and notify"), UpdateSettings::Notified);
        const auto task = m_checker.task();
        if (task->installs()) {
            m_mode->addItem(tr("Automatic"), UpdateSettings::Automatic);
        }
        m_mode->setCurrentIndex(std::max(0, m_mode->findData(storedMode())));
        form->addRow(tr("Update &mode:"), m_mode);
        auto modes = tr("Manual: updates are checked only from Help > Check for Updates.\n"
                        "Check and notify: updates are checked at startup, at most once a day, "
                        "and a new version is offered.");
        if (task->installs()) {
            modes += QLatin1Char('\n') +
                     tr("Automatic: updates are checked at startup as well, and a new version is "
                        "downloaded in the background and installed when the application quits.");
        }
        form->addRow(note(modes));
        m_source = new QComboBox(widget);
        // The names of the sources are those of the update manifest, untranslated.
        m_source->addItem(tr("Automatic"), QString());
        if (const auto &manifest = m_checker.manifest()) {
            for (const auto &source : manifest->sources) {
                m_source->addItem(source.name, source.name);
            }
        }
        m_source->setCurrentIndex(std::max(0, m_source->findData(storedSource())));
        form->addRow(tr("Update &source:"), m_source);
        form->addRow(note(tr("Automatic: the source is selected by the time zone of the system.")));
        m_extractionThreads = new QSpinBox(widget);
        m_extractionThreads->setObjectName(QStringLiteral("extractionThreads"));
        m_extractionThreads->setRange(1, 64);
        m_extractionThreads->setValue(m_settings.updates().extractionThreadCount());
        form->addRow(tr("&Extraction threads:"), m_extractionThreads);
        const auto threadsNote = note(tr("The number of threads that extract the archive of an "
                                         "update of the portable version. The default is %1.")
                                          .arg(UpdateSettings::defaultExtractionThreadCount));
        form->addRow(threadsNote);
        // Only the archive of the portable version is extracted.
        if (task->kind() != UpdateTask::Portable) {
            form->setRowVisible(m_extractionThreads, false);
            form->setRowVisible(threadsNote, false);
        }
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
        connect(m_extractionThreads, &QSpinBox::valueChanged, this, &SettingPage::modifiedChanged);
        layout->addWidget(group);
        layout->addStretch();
        return widget;
    }

    bool UpdateSettingPage::isModified() const {
        if (!m_mode) {
            return false;
        }
        return m_mode->currentData().toInt() != storedMode() ||
               m_source->currentData().toString() != storedSource() ||
               m_extractionThreads->value() != m_settings.updates().extractionThreadCount();
    }

    bool UpdateSettingPage::apply(QString *error) {
        Q_UNUSED(error);
        m_settings.updates().setMode(UpdateSettings::Mode(m_mode->currentData().toInt()));
        m_settings.updates().setSource(m_source->currentData().toString());
        m_settings.updates().setExtractionThreadCount(m_extractionThreads->value());
        Q_EMIT modifiedChanged();
        return true;
    }

    QComboBox *UpdateSettingPage::modeBox() const {
        return m_mode;
    }

    QComboBox *UpdateSettingPage::sourceBox() const {
        return m_source;
    }

    UpdateSettings::Mode UpdateSettingPage::storedMode() const {
        // An application that does not install updates treats the mode Automatic as Check and
        // notify.
        const auto mode = m_settings.updates().mode();
        return mode == UpdateSettings::Automatic && !m_checker.task()->installs()
                   ? UpdateSettings::Notified
                   : mode;
    }

    QString UpdateSettingPage::storedSource() const {
        const auto source = m_settings.updates().source();
        const auto &manifest = m_checker.manifest();
        return manifest && manifest->hasSource(source) ? source : QString();
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
