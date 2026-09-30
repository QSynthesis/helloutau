#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <stdcorelib/pluginsystem/iplugin.h>

#include <helloutau/Editor/AppLoader.h>

namespace {

    /// A plugin for test_AppLoader that appends its calls to the property "appLoaderEvents" of the
    /// application. Built with TEST_APPLOADER_FAILS, its initialization fails.
    ///
    /// The events are copied from UTF-8 rather than written with QStringLiteral, whose text stays
    /// in this library and is gone once the library is unloaded, while the test still reads it.
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
