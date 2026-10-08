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
                                  "the plugins folder of the UTAU folder in System Settings."),
                               group);
        note->setWordWrap(true);
        groupLayout->addWidget(note);
        for (const auto &folder : ClassicPluginContribution::pluginFolders(m_settings)) {
            auto path = new QLineEdit(
                QDir::toNativeSeparators(QString::fromStdU16String(folder.u16string())), group);
            path->setReadOnly(true);
            auto open = new QPushButton(tr("Open"), group);
            connect(open, &QPushButton::clicked, group,
                    [folder] { ClassicPluginContribution::openFolder(folder); });
            auto row = new QHBoxLayout();
            row->addWidget(path, 1);
            row->addWidget(open);
            groupLayout->addLayout(row);
        }
        layout->addWidget(group);
        layout->addStretch();
        return widget;
    }

}
