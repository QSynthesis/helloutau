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

    /// One setting supported by a driver, described in sufficient detail to generate a form.
    ///
    /// Intended for settings that map to a single control. A setting that requires a custom
    /// view, such as selecting a text encoding with a live preview, is implemented as a custom
    /// step of the driver instead. See \c InterchangeReader::customStepId().
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

    /// The capacity of the import destination, which is known to the caller but not to the
    /// driver.
    ///
    /// The driver knows the number of entries in the file. Whether the project can accommodate
    /// them is a separate matter, and the answer changes once multiple tracks are supported,
    /// without any change to the drivers.
    struct ImportLimits {
        int minEntries = 1;
        int maxEntries = 1; ///< a project currently holds one track
    };

    /// The settings chosen by the user, which the conversion follows.
    struct ImportRequest {
        /// The entries to import, by \c InterchangeEntry::index.
        QList<int> entries;

        /// The values of the settings declared by the driver, by \c InterchangeOption::key. A
        /// driver ignores unrecognized keys.
        QVariantMap driverOptions;
    };

    struct ExportRequest {
        QVariantMap driverOptions;
    };

    /// The result of an import, which distinguishes the two reasons for an empty result.
    ///
    /// Both a user who closed the selector and a file that could not be read leave \c project
    /// empty, and the two require opposite handling by the caller. A cancellation is not an
    /// error and must not produce a message box. A read failure is an error and must.
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
