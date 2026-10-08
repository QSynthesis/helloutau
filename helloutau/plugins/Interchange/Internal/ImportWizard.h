#ifndef HELLOUTAU_INTERCHANGE_IMPORTWIZARD_H
#define HELLOUTAU_INTERCHANGE_IMPORTWIZARD_H

#include <filesystem>
#include <optional>

#include <QtWidgets/QWizard>

#include <hellokit/Interchange/InterchangeSource.h>
#include <hellokit/Interchange/InterchangeRequest.h>

#include <Interchange/ImportMerge.h>
#include <Interchange/InterchangeStepRegistry.h>

namespace hello::kit {
    class InterchangeReader;
    class InterchangeDrivers;
}

namespace hello::daw {

    class InterchangeService;
    class ProjectWindow;

    /// Wizard that imports a file in a registered format into the project of a window. Pages:
    /// file and format, driver options, entry, insertion position, and result. See
    /// docs/ImportExport.md.
    class ImportWizard : public QWizard {
        Q_OBJECT
    public:
        enum Page {
            FilePage,
            OptionsPage,
            EntriesPage,
            PositionPage,
            ResultPage,
        };

        ImportWizard(ProjectWindow *window, InterchangeService *service);
        ~ImportWizard() override;

        /// The choices collected by the pages, in page order.
        struct State {
            ProjectWindow *window = nullptr;
            kit::InterchangeDrivers *drivers = nullptr;
            InterchangeStepRegistry *stepPages = nullptr;
            std::filesystem::path path;
            kit::InterchangeReader *reader = nullptr;
            std::optional<kit::InterchangeSource> source;
            kit::ImportRequest request;
            ImportMerge::Options merge;
            // Diagnostics of the pages, listed on the result page before those of the import
            kit::DiagnosticList diagnostics;
            // Whether the project has notes. Determines the default of keepLeadingRest.
            bool projectHasNotes = false;
        };

        State &state();

    private:
        State m_state;
    };

}

#endif // HELLOUTAU_INTERCHANGE_IMPORTWIZARD_H
