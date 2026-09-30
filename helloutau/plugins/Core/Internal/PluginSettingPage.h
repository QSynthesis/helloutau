#ifndef HELLOUTAU_CORE_PLUGINSETTINGPAGE_H
#define HELLOUTAU_CORE_PLUGINSETTINGPAGE_H

#include <QtCore/QPointer>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Widgets/SettingPage.h>

class QLabel;
class QTreeWidget;

namespace hello::daw {

    /// The plugins that the loader found, what became of each in this run, and whether the user
    /// enables it, which takes effect at the next start. See docs/Plugins.md.
    ///
    /// The core plugin cannot be disabled, since the application does not start without it.
    class PluginSettingPage : public SettingPage {
        Q_OBJECT
    public:
        explicit PluginSettingPage(AppLoader &loader, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

    protected:
        QWidget *createWidget() override;

    private:
        AppLoader &m_loader;
        // The plugins as they were when the widget was created, in the order of the rows
        QList<AppLoader::PluginInfo> m_plugins;
        QPointer<QTreeWidget> m_tree;
        QPointer<QLabel> m_restart;
        QPointer<QLabel> m_details;

        // Whether the user enables the plugin of \a row at the next start, as the settings are
        bool enabledAtNextStart(int row) const;
        bool isChecked(int row) const;
        void updateRestart();
        void updateDetails();
    };

}

#endif // HELLOUTAU_CORE_PLUGINSETTINGPAGE_H
