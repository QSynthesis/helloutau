#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <stdcorelib/pluginsystem/iplugin.h>

#include <helloutau/Editor/AppLoader.h>

namespace {

    /// Test plugin of test_AppLoader that appends its calls to the property \c appLoaderEvents
    /// of the application. If built with TEST_APPLOADER_FAILS, the plugin fails to initialize.
    ///
    /// The events are copied from UTF-8 instead of being created with QStringLiteral. The text of
    /// a QStringLiteral is stored in this library and becomes invalid when the library is
    /// unloaded, while the test still reads the events.
    class TestAppLoaderPlugin : public stdc::pluginsystem::IPlugin {
    public:
        bool initialize(std::string *errorMessage) override {
#ifdef TEST_APPLOADER_FAILS
            *errorMessage = "intentional failure";
            return false;
#else
            (void) errorMessage;
            record(QString::fromUtf8("initialize ") +
                   hello::daw::AppLoader::instance()->files().join(QLatin1Char(' ')));
            return true;
#endif
        }

        void pluginsInitialized() override {
            record(QString::fromUtf8("pluginsInitialized"));
        }

        void aboutToShutdown() override {
            record(QString::fromUtf8("aboutToShutdown"));
        }

    private:
        static void record(const QString &event) {
            auto events = qApp->property("appLoaderEvents").toStringList();
            events.push_back(event);
            qApp->setProperty("appLoaderEvents", events);
        }
    };

}

STDC_EXPORT_PLUGIN(TestAppLoaderPlugin)
