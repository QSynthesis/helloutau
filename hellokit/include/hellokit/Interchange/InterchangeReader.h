#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEREADER_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEREADER_H

#include <filesystem>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Document/Project.h>
#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeRequest.h>
#include <hellokit/Interchange/InterchangeSource.h>

namespace hello::kit {

    class InterchangeSelector;

    /// Imports one foreign format into a project.
    ///
    /// Import and export are separate classes rather than one class with a capability flag.
    /// Most formats support import only, and a single class would leave the unsupported
    /// direction returning false, explained only by the documentation.
    ///
    /// A driver translates and makes no decisions. All content of the file is converted,
    /// including the tempo and the silence before the first note, even if the caller discards
    /// it afterward. Decisions on behalf of the user belong to the import flow of the
    /// application, the only component that knows the destination of the notes.
    ///
    /// \sa docs/Interchange.md
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeReader {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::InterchangeReader)
    public:
        virtual ~InterchangeReader();

        virtual QString id() const = 0;           ///< \c "midi"
        virtual QString name() const = 0;         ///< \c "Standard MIDI File"
        virtual QStringList suffixes() const = 0; ///< \c {"mid", "midi"}, without the dot

        /// The settings accepted by this driver, from which a form is generated.
        virtual QList<InterchangeOption> optionSchema() const;

        /// The ID of a custom view for this driver, for settings a generated form cannot
        /// represent.
        ///
        /// Empty for a driver whose \c optionSchema() is sufficient. If set, the view is
        /// registered separately on the widgets side under the same ID, because a driver in
        /// this module cannot display anything.
        virtual QString customStepId() const;

        /// Determines the contents of \a path without converting anything.
        virtual std::optional<InterchangeSource> inspect(const std::filesystem::path &path,
                                                         DiagnosticList &diagnostics) = 0;

        /// Inspects \a path , obtains the import settings from \a selector , and converts.
        ///
        /// The entire flow is deliberately a single call. A caller that performed the three
        /// steps itself would be an additional place where their order could differ, whereas
        /// the command line, the tests and the editor must import identically.
        ///
        /// \param selector the source of user decisions, or null to accept every default
        ///        through \c AutomaticSelector
        ImportResult read(const std::filesystem::path &path, InterchangeSelector *selector,
                          const ImportLimits &limits = {});

    protected:
        /// Converts the file as specified by \a request . Called by read() once all settings
        /// are determined.
        virtual std::optional<Project> convert(const std::filesystem::path &path,
                                               const InterchangeSource &source,
                                               const ImportRequest &request,
                                               DiagnosticList &diagnostics) = 0;
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEREADER_H
