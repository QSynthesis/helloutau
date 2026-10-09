#ifndef HELLOUTAU_INTERCHANGE_INTERNAL_EXPORTWIZARD_H
#define HELLOUTAU_INTERCHANGE_INTERNAL_EXPORTWIZARD_H

#include <filesystem>

#include <QtWidgets/QWizard>

#include <hellokit/Interchange/InterchangeRequest.h>

namespace hello::kit {
    class InterchangeWriter;
    class InterchangeDrivers;
}

namespace hello::daw {

    class ProjectWindow;

    /// Wizard that exports the project of a window to a file in a registered format. Pages: file
    /// and format, driver options, and result. See docs/ImportExport.md.
    class ExportWizard : public QWizard {
        Q_OBJECT
    public:
        enum Page {
            FilePage,
            OptionsPage,
            ResultPage,
        };

        ExportWizard(ProjectWindow *window, kit::InterchangeDrivers *drivers);
        ~ExportWizard() override;

        /// The choices collected by the pages, in page order.
        struct State {
            ProjectWindow *window = nullptr;
            kit::InterchangeDrivers *drivers = nullptr;
            std::filesystem::path path;
            kit::InterchangeWriter *writer = nullptr;
            kit::ExportRequest request;
        };

        State &state();

    private:
        State m_state;
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERNAL_EXPORTWIZARD_H
