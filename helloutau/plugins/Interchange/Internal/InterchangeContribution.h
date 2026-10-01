#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGECONTRIBUTION_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGECONTRIBUTION_H

#include <helloutau/Editor/ActionContribution.h>

namespace hello::kit {
    class InterchangeRegistry;
}

namespace hello::daw {

    /// The commands Import and Export > Other Formats in the File menu of the project windows.
    /// Each command opens a wizard for the formats of the registry. See docs/ImportExport.md.
    class InterchangeContribution : public ActionContribution {
    public:
        explicit InterchangeContribution(kit::InterchangeRegistry *registry);
        ~InterchangeContribution();

        const QAK::ActionExtension *extension(Editor::WindowKind kind) const override;
        void addActions(ProjectWindow *window, QAK::WidgetActionContext *context) override;

    private:
        kit::InterchangeRegistry *m_registry;
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGECONTRIBUTION_H
