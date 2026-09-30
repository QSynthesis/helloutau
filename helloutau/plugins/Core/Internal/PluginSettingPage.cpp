#include "PluginSettingPage.h"

#include <QtCore/QDir>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    namespace {

        enum Column {
            NameColumn,
            VersionColumn,
            StateColumn,
        };

        bool isCore(const AppLoader::PluginInfo &info) {
            return info.id == QLatin1String(AppLoader::corePluginId);
        }

        QString nameOf(const AppLoader::PluginInfo &info) {
            return info.displayName.isEmpty() ? info.id : info.displayName;
        }

        QString stateText(const AppLoader::PluginInfo &info) {
            switch (info.state) {
                case AppLoader::PluginInfo::Running:
                    return PluginSettingPage::tr("Running");
                case AppLoader::PluginInfo::Disabled:
                    return PluginSettingPage::tr("Disabled");
                case AppLoader::PluginInfo::Failed:
                    return PluginSettingPage::tr("Error");
                case AppLoader::PluginInfo::NotLoaded:
                    break;
            }
            return PluginSettingPage::tr("Not loaded");
        }

    }

    PluginSettingPage::PluginSettingPage(AppLoader &loader, QObject *parent)
        : SettingPage(QStringLiteral("core.Plugins"), parent), m_loader(loader) {
        setTitle(tr("Plugins"));
        setDescription(tr("The plugins found, and which of them run. A plugin enabled or disabled "
                          "here runs or stops at the next start."));
        setKeywords({QStringLiteral("Plugins"), QStringLiteral("extensions")});
    }

    QWidget *PluginSettingPage::createWidget() {
        m_plugins = m_loader.plugins();

        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);
        m_restart = new QLabel(tr("Restart %1 for the plugins enabled or disabled to run or stop.")
                                   .arg(QCoreApplication::applicationName()));
        m_restart->setWordWrap(true);
        layout->addWidget(m_restart);

        m_tree = new QTreeWidget();
        m_tree->setRootIsDecorated(false);
        m_tree->setUniformRowHeights(true);
        m_tree->setHeaderLabels({tr("Name"), tr("Version"), tr("State")});
        m_tree->header()->setStretchLastSection(false);
        m_tree->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
        m_tree->header()->setSectionResizeMode(VersionColumn, QHeaderView::ResizeToContents);
        m_tree->header()->setSectionResizeMode(StateColumn, QHeaderView::ResizeToContents);
        const auto warning = widget->style()->standardIcon(QStyle::SP_MessageBoxWarning);
        for (int row = 0; row < m_plugins.size(); ++row) {
            const auto &info = m_plugins[row];
            auto item = new QTreeWidgetItem(m_tree);
            item->setText(NameColumn, nameOf(info));
            item->setText(VersionColumn, info.version);
            item->setText(StateColumn, stateText(info));
            item->setCheckState(NameColumn, enabledAtNextStart(row) ? Qt::Checked : Qt::Unchecked);
            if (isCore(info)) {
                // Shown checked, and not to be unchecked
                item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
                item->setToolTip(NameColumn,
                                 tr("The application does not start without the core plugin."));
            }
            if (info.state == AppLoader::PluginInfo::Failed) {
                item->setIcon(StateColumn, warning);
                item->setToolTip(StateColumn, info.error);
            }
        }
        layout->addWidget(m_tree, 1);

        m_details = new QLabel();
        m_details->setWordWrap(true);
        m_details->setTextFormat(Qt::PlainText);
        m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(m_details);

        connect(m_tree, &QTreeWidget::itemChanged, this, [this] {
            updateRestart();
            Q_EMIT modifiedChanged();
        });
        connect(m_tree, &QTreeWidget::currentItemChanged, this, &PluginSettingPage::updateDetails);
        if (m_tree->topLevelItemCount() > 0) {
            m_tree->setCurrentItem(m_tree->topLevelItem(0));
        }
        updateRestart();
        updateDetails();
        return widget;
    }

    bool PluginSettingPage::isModified() const {
        if (!m_tree) {
            return false;
        }
        for (int row = 0; row < m_plugins.size(); ++row) {
            if (!isCore(m_plugins[row]) && isChecked(row) != enabledAtNextStart(row)) {
                return true;
            }
        }
        return false;
    }

    // A choice equal to what the metadata says is no choice, so that plugins.json keeps only
    // the plugins that the user changed.
    bool PluginSettingPage::apply(QString *error) {
        Q_UNUSED(error);
        for (int row = 0; row < m_plugins.size(); ++row) {
            const auto &info = m_plugins[row];
            if (isCore(info)) {
                continue;
            }
            const bool checked = isChecked(row);
            m_loader.setPluginEnabled(info.id, checked == info.enabledByDefault
                                                   ? std::nullopt
                                                   : std::optional<bool>(checked));
        }
        updateRestart();
        Q_EMIT modifiedChanged();
        return true;
    }

    bool PluginSettingPage::enabledAtNextStart(int row) const {
        const auto &info = m_plugins[row];
        return m_loader.pluginEnabled(info.id).value_or(info.enabledByDefault);
    }

    bool PluginSettingPage::isChecked(int row) const {
        return m_tree->topLevelItem(row)->checkState(NameColumn) == Qt::Checked;
    }

    // Shown while a plugin is checked otherwise than it was enabled in this run, applied or not
    void PluginSettingPage::updateRestart() {
        bool differs = false;
        for (int row = 0; row < m_plugins.size(); ++row) {
            differs = differs || isChecked(row) != m_plugins[row].enabled;
        }
        m_restart->setVisible(differs);
    }

    void PluginSettingPage::updateDetails() {
        const auto item = m_tree->currentItem();
        if (!item) {
            m_details->clear();
            return;
        }
        const auto &info = m_plugins[m_tree->indexOfTopLevelItem(item)];

        QStringList dependsOn;
        for (const auto &dependency : info.dependencies) {
            dependsOn.push_back(dependency.optional ? tr("%1 (optional)").arg(dependency.id)
                                                    : dependency.id);
        }
        QStringList requiredBy;
        for (const auto &other : std::as_const(m_plugins)) {
            for (const auto &dependency : other.dependencies) {
                if (dependency.id == info.id) {
                    requiredBy.push_back(dependency.optional ? tr("%1 (optional)").arg(other.id)
                                                             : other.id);
                }
            }
        }
        const auto listOf = [](const QStringList &ids) {
            return ids.isEmpty() ? tr("None") : ids.join(QStringLiteral(", "));
        };

        QStringList lines = {
            tr("ID: %1").arg(info.id),
            tr("Library: %1").arg(QDir::toNativeSeparators(info.filePath)),
            tr("Depends on: %1").arg(listOf(dependsOn)),
            tr("Needed by: %1").arg(listOf(requiredBy)),
        };
        if (!info.error.isEmpty()) {
            lines.push_back(tr("Error: %1").arg(info.error));
        }
        m_details->setText(lines.join(QLatin1Char('\n')));
    }

}
