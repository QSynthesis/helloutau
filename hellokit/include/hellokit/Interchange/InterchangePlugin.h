#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEPLUGIN_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEPLUGIN_H

#include <memory>
#include <vector>

#include <QtPlugin>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

namespace hello::kit {

    /// Adds formats to the application.
    ///
    /// What it supplies is drivers, not user interface. A plugin whose driver needs a view of
    /// its own supplies that separately on the widgets side, registered under the id its driver
    /// returns from \c customStepId(), and a plugin that only ships this half still works with
    /// a generated form.
    ///
    /// \note The drivers are handed over, not lent. A plugin does not keep a pointer to what it
    ///       returned here, since the registry it goes into may outlive nothing in particular
    ///       and is free to destroy them.
    class HELLOKIT_INTERCHANGE_EXPORT InterchangePlugin {
    public:
        virtual ~InterchangePlugin();

        virtual std::vector<std::unique_ptr<InterchangeReader>> createReaders();
        virtual std::vector<std::unique_ptr<InterchangeWriter>> createWriters();
    };

}

Q_DECLARE_INTERFACE(hello::kit::InterchangePlugin, "org.qsynthesis.HelloUTAU.InterchangePlugin/1.0")

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEPLUGIN_H
