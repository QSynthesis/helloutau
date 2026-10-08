#include "ClassicPluginHostPlugin.h"

#include <QtCore/QtGlobal>

#include <helloutau/Editor/AppLoader.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/EditorSettingPageIds.h>
#include <helloutau/Editor/Translations.h>
#include <helloutau/Widgets/SettingPageRegistration.h>

#include "ClassicPluginContribution.h"
#include "ClassicPluginSettingPage.h"

namespace hello::daw {

    namespace {

        const char pluginId[] = "org.helloutau.classicpluginhost";

    }

    ClassicPluginHostPlugin::ClassicPluginHostPlugin() = default;

    ClassicPluginHostPlugin::~ClassicPluginHostPlugin() = default;

    bool ClassicPluginHostPlugin::initialize(std::string *errorMessage) {
        Translations::load(QStringLiteral("ClassicPluginHost"),
                           QStringLiteral(":/helloutau/plugins/ClassicPluginHost/translations"));
        // The core plugin, on which this plugin depends, has created the editor.
        const auto loader = AppLoader::instance();
        const auto editor = loader ? loader->editor() : nullptr;
        if (!editor) {
            if (errorMessage) {
                *errorMessage = "The editor of the core plugin does not exist.";
            }
            return false;
        }
        m_registration =
            ActionContributionRegistry::AddFactory(editor->actionContributions(), pluginId, {}, [] {
                return std::make_unique<ClassicPluginContribution>();
            });
        if (!m_registration.entry()) {
            if (errorMessage) {
                *errorMessage = "The actions of the plugin are already registered.";
            }
            return false;
        }
        // The page follows the Plugins page of the core plugin, which also precedes Rendering.
        m_settingPage = std::make_unique<SettingPageRegistration>(
            editor->settingPageRegistry(),
            [](QObject *host) -> SettingPage * {
                const auto editor = qobject_cast<Editor *>(host);
                return editor ? new ClassicPluginSettingPage(editor->settings()) : nullptr;
            },
            QString(), QLatin1String(EditorSettingPageIds::rendering));
        return true;
    }

    void ClassicPluginHostPlugin::aboutToShutdown() {
        m_settingPage.reset();
        m_registration = {};
    }

}

STDC_EXPORT_PLUGIN(hello::daw::ClassicPluginHostPlugin)
