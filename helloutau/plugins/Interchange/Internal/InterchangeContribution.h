#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGECONTRIBUTION_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGECONTRIBUTION_H

#include <helloutau/Widgets/ActionContribution.h>

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

        const QAK::ActionExtension *extension(const QString &windowKind) const override;
        void addActions(QWidget *widget, QAK::WidgetActionContext *context) override;

    private:
        kit::InterchangeRegistry *m_registry;
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGECONTRIBUTION_H
