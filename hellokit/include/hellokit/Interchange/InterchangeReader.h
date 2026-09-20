#ifndef HELLOKIT_INTERCHANGE_INTERCHANGEREADER_H
#define HELLOKIT_INTERCHANGE_INTERCHANGEREADER_H

#include <filesystem>
#include <optional>

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Document/Project.h>
#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Interchange/HelloKitInterchangeGlobal.h>
#include <hellokit/Interchange/InterchangeRequest.h>
#include <hellokit/Interchange/InterchangeSource.h>

namespace hello::kit {

    class InterchangeSelector;

    /// Turns one foreign format into a project.
    ///
    /// Reading and writing are separate classes rather than one class with a flag saying which
    /// half works. Most formats can only be read, and a single class would leave the other half
    /// returning false with only the documentation to say why.
    ///
    /// A driver translates and does not choose. Whatever the file says comes across, including
    /// the tempo and the silence in front of the first note, even where the caller is about to
    /// throw it away. Deciding what the user wants belongs to the import flow in the
    /// application, which is the only place that knows where the notes are going.
    ///
    /// \sa docs/Interchange.md
    class HELLOKIT_INTERCHANGE_EXPORT InterchangeReader {
    public:
        virtual ~InterchangeReader();

        virtual QString id() const = 0;           ///< \c "midi"
        virtual QString name() const = 0;         ///< \c "Standard MIDI File"
        virtual QStringList suffixes() const = 0; ///< \c {"mid", "midi"}, without the dot

        /// The settings this driver takes, for a form to be generated from.
        virtual QList<InterchangeOption> optionSchema() const;

        /// The id of a view of this driver's own, where a generated form will not do.
        ///
        /// Empty for a driver whose \c optionSchema() covers everything. Where it is set, the
        /// half of the step that draws is registered separately on the widgets side under this
        /// same id, since a driver cannot draw anything from here.
        virtual QString customStepId() const;

        /// Works out what \a path holds without converting any of it.
        virtual std::optional<InterchangeSource> inspect(const std::filesystem::path &path,
                                                         DiagnosticList &diagnostics) = 0;

        /// Inspects \a path, asks \a selector what to do, and converts.
        ///
        /// The whole flow in one call on purpose. A caller that ran the three steps itself would
        /// be a fourth place where they could be ordered differently, and the command line, the
        /// tests and the editor are supposed to import in exactly the same way.
        ///
        /// \param selector where the user's answers come from, or null to take every default
        ///        through \c AutomaticSelector
        ImportResult read(const std::filesystem::path &path, InterchangeSelector *selector,
                          const ImportLimits &limits = {});

    protected:
        /// Converts what \a request asked for. Called by read() once the questions are answered.
        virtual std::optional<Project> convert(const std::filesystem::path &path,
                                               const InterchangeSource &source,
                                               const ImportRequest &request,
                                               DiagnosticList &diagnostics) = 0;
    };

}

#endif // HELLOKIT_INTERCHANGE_INTERCHANGEREADER_H
