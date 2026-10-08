#ifndef HELLOUTAU_CORE_PLUGINSETTINGPAGE_H
#define HELLOUTAU_CORE_PLUGINSETTINGPAGE_H

#include <vector>

#include <QtCore/QPointer>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Widgets/SettingPage.h>

#include <Core/CorePluginGlobal.h>

class QLabel;
class QPlainTextEdit;
class QTreeWidget;

namespace hello::daw {

    /// Setting page that lists the plugins found by the loader, the state of each plugin in this
    /// run, and whether the user enables it. A change takes effect at the next start. See
    /// docs/Plugins.md.
    ///
    /// The core plugin cannot be disabled because the application requires it in order to start.
    class COREPLUGIN_EXPORT PluginSettingPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "core.Plugins";

        explicit PluginSettingPage(AppLoader &loader, QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

    protected:
        QWidget *createWidget() override;

    private:
        AppLoader &m_loader;
        // The plugins at the creation of the widget, in row order
        std::vector<stdc::pluginsystem::PluginSpec *> m_plugins;
        QPointer<QTreeWidget> m_tree;
        QPointer<QLabel> m_restart;
        QPointer<QPlainTextEdit> m_details;

        // Returns whether the plugin of \a row is enabled at the next start according to the
        // current settings.
        bool enabledAtNextStart(int row) const;
        bool isChecked(int row) const;
        void updateRestart();
        void updateDetails();
    };

}

#endif // HELLOUTAU_CORE_PLUGINSETTINGPAGE_H
