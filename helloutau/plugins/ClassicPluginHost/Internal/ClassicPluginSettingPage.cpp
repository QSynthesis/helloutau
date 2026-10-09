#include "ClassicPluginSettingPage.h"

#include <QtCore/QDir>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "ClassicPluginContribution.h"

namespace hello::daw {

    ClassicPluginSettingPage::ClassicPluginSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_settings(settings) {
        setTitle(tr("Classic Plugins"));
        setDescription(tr("The folders in which the plugins of UTAU are discovered."));
        setKeywords(
            {QStringLiteral("Classic Plugins"), QStringLiteral("UTAU"), QStringLiteral("Plugins")});
    }

    QWidget *ClassicPluginSettingPage::createWidget() {
        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);
        auto group = new QGroupBox(tr("Plugin folders"), widget);
        auto groupLayout = new QVBoxLayout(group);
        auto note = new QLabel(tr("The plugins are discovered in these folders, in this order. A "
                                  "folder that does not exist is skipped. The second folder is "
                                  "the plugins folder of the UTAU folder on the UTAU page."),
                               group);
        note->setWordWrap(true);
        groupLayout->addWidget(note);
        m_folders = new QWidget(group);
        auto foldersLayout = new QVBoxLayout(m_folders);
        foldersLayout->setContentsMargins(0, 0, 0, 0);
        groupLayout->addWidget(m_folders);
        fillFolders();
        layout->addWidget(group);
        layout->addStretch();
        return widget;
    }

    void ClassicPluginSettingPage::settingsApplied() {
        fillFolders();
    }

    void ClassicPluginSettingPage::fillFolders() {
        if (!m_folders) {
            return;
        }
        qDeleteAll(m_folders->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly));
        for (const auto &folder : ClassicPluginContribution::pluginFolders(m_settings)) {
            auto row = new QWidget(m_folders);
            auto rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            auto path = new QLineEdit(
                QDir::toNativeSeparators(QString::fromStdU16String(folder.u16string())), row);
            path->setReadOnly(true);
            auto open = new QPushButton(tr("Open"), row);
            connect(open, &QPushButton::clicked, row,
                    [folder] { ClassicPluginContribution::openFolder(folder); });
            rowLayout->addWidget(path, 1);
            rowLayout->addWidget(open);
            m_folders->layout()->addWidget(row);
        }
    }

}
