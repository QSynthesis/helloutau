#include "ClassicPluginHostPlugin.h"

#include <QtCore/QtGlobal>

#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/EditorSettingPageIds.h>
#include <helloutau/Editor/Translations.h>
#include <helloutau/Widgets/ActionRegistration.h>
#include <helloutau/Widgets/SettingPageRegistration.h>

#include "ClassicPluginContribution.h"
#include "ClassicPluginSettingPage.h"

namespace hello::daw {

    ClassicPluginHostPlugin::ClassicPluginHostPlugin() = default;

    ClassicPluginHostPlugin::~ClassicPluginHostPlugin() = default;

    bool ClassicPluginHostPlugin::initialize(std::string *errorMessage) {
        Q_UNUSED(errorMessage);
        Translations::load(QStringLiteral("ClassicPluginHost"),
                           QStringLiteral(":/helloutau/plugins/ClassicPluginHost/translations"));
        m_registration =
            std::make_unique<ActionRegistration>(std::make_unique<ClassicPluginContribution>());
        // The page follows the Plugins page of the core plugin, which also precedes Rendering.
        m_settingPage = std::make_unique<SettingPageRegistration>(
            [](QObject *host) -> SettingPage * {
                const auto editor = qobject_cast<Editor *>(host);
                return editor ? new ClassicPluginSettingPage(editor->settings()) : nullptr;
            },
            QString(), QLatin1String(EditorSettingPageIds::rendering));
        return true;
    }

    void ClassicPluginHostPlugin::aboutToShutdown() {
        m_settingPage.reset();
        m_registration.reset();
    }

}

STDC_EXPORT_PLUGIN(hello::daw::ClassicPluginHostPlugin)
