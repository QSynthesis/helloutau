// The plugin is built into this library rather than loaded from a plugin directory. This macro
// therefore makes moc emit the function by which Q_IMPORT_PLUGIN registers the plugin
// (qtbase/src/corelib/plugin/qplugin.h). The macro must precede every Qt header.
#define QT_STATICPLUGIN

#include <QtCore/QtPlugin>
#include <QtGui/QIconEnginePlugin>

#include "ThemeIconEngine_p.h"
#include "ThemeLogging_p.h"

namespace hello::daw {

    /// The icon engine plugin for file names ending in \c .svgx. QIcon selects an engine by the
    /// suffix of the file name (qtbase/src/gui/image/qicon.cpp).
    class ThemeIconPlugin : public QIconEnginePlugin {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID QIconEngineFactoryInterface_iid FILE "ThemeIconPlugin.json")
    public:
        QIconEngine *create(const QString &fileName) override {
            auto icon = ThemeIcon::fromFileName(fileName);
            if (!icon) {
                qCWarning(lcTheme) << "The icon" << fileName << "cannot be read.";
            }
            return new ThemeIconEngine(icon.value_or(ThemeIcon()));
        }
    };

}

#include "ThemeIconPlugin.moc"

Q_IMPORT_PLUGIN(ThemeIconPlugin)
