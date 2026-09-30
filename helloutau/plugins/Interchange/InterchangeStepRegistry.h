#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRY_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRY_H

#include <QtCore/QString>

#include <Interchange/InterchangePluginGlobal.h>

class QWidget;

namespace hello::daw {

    class InterchangeStepPage;

    /// The custom step pages registered in the process by InterchangeStepRegistration. See the
    /// custom selection steps in docs/Interchange.md.
    ///
    /// The registry holds no state. Its content is the process-wide list of registrations at
    /// the time of each call.
    class INTERCHANGEPLUGIN_EXPORT InterchangeStepRegistry {
    public:
        /// Returns whether a page is registered for \a id.
        static bool contains(const QString &id);

        /// Returns a new page of the first registration for \a id, parented to \a parent, or
        /// null if no page is registered for \a id.
        static InterchangeStepPage *create(const QString &id, QWidget *parent = nullptr);
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGESTEPREGISTRY_H
