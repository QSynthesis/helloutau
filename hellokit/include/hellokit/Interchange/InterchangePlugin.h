#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEPLUGIN_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEPLUGIN_H

#include <memory>
#include <vector>

#include <QtCore/QtPlugin>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    /// Provides additional formats to the application.
    ///
    /// A plugin provides drivers, not user interface. A plugin whose driver requires a custom
    /// view provides it separately on the widgets side, registered under the ID returned by
    /// \c customStepId() of the driver. A plugin that provides only the drivers still works
    /// with a generated form.
    ///
    /// \note Ownership of the drivers is transferred. A plugin must not retain pointers to the
    ///       returned drivers, because the registry may destroy them at any time.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangePlugin {
    public:
        virtual ~InterchangePlugin();

        virtual std::vector<std::unique_ptr<InterchangeReader>> createReaders();
        virtual std::vector<std::unique_ptr<InterchangeWriter>> createWriters();
    };

}

Q_DECLARE_INTERFACE(hello::kit::InterchangePlugin, "org.qsynthesis.HelloUtau.InterchangePlugin/1.0")

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEPLUGIN_H
