// The plugin is built into this library, not loaded from a folder of plugins, so moc emits the
// function by which Q_IMPORT_PLUGIN registers it (qtbase/src/corelib/plugin/qplugin.h). The
// macro must precede every Qt header.
#define QT_STATICPLUGIN

#include <QtCore/QtPlugin>
#include <QtGui/QIconEnginePlugin>

#include "ThemeIconEngine_p.h"
#include "ThemeLogging_p.h"

namespace hello::daw {

    /// Creates the icon engine for file names ending in \c .svgx, as QIcon chooses an engine by
    /// the suffix of the file name (qtbase/src/gui/image/qicon.cpp).
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
