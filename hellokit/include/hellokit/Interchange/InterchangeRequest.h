#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEREQUEST_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEREQUEST_H

#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>

#include <hellokit/Document/Project.h>
#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>

namespace hello::kit {

    /// One setting a driver understands, described well enough for a form to be built from it.
    ///
    /// For settings where one control asks one question. Anything that needs a view of its own,
    /// such as picking a text encoding against a live preview, is a step of the driver's own
    /// instead. See \c InterchangeReader::customStepId().
    struct InterchangeOption {
        enum Type {
            Boolean,
            Integer,
            Choice, ///< one of \c choices
            Text,
        };

        QString key;
        QString name; ///< shown to the user
        Type type = Boolean;
        QVariant defaultValue;
        QStringList choices;
    };

    /// How much the destination can take, which the caller knows and the driver does not.
    ///
    /// The driver knows how many entries the file holds. Whether the project can hold them is a
    /// different question, and the answer moves when several tracks become possible without any
    /// driver changing.
    struct ImportLimits {
        int minEntries = 1;
        int maxEntries = 1; ///< a project holds one track for now
    };

    /// What the user settled on, which is what the conversion then follows.
    struct ImportRequest {
        /// Which entries to bring in, by \c InterchangeEntry::index.
        QList<int> entries;

        /// The settings the driver declared, by \c InterchangeOption::key. A driver ignores what
        /// it does not recognize.
        QVariantMap driverOptions;
    };

    struct ExportRequest {
        QVariantMap driverOptions;
    };

    /// The outcome of an import, where nothing came back for one of two different reasons.
    ///
    /// A user who closed the chooser and a file that could not be read both leave \c project
    /// empty, and the two want opposite things from the caller: one is not an error and must not
    /// raise a message box, the other is and must.
    struct ImportResult {
        std::optional<Project> project;
        bool cancelled = false;
        DiagnosticList diagnostics;
    };

    struct ExportResult {
        bool written = false;
        bool cancelled = false;
        DiagnosticList diagnostics;
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEREQUEST_H
