#ifndef HELLOUTAU_INTERCHANGE_INTERNAL_INTERCHANGECONTRIBUTION_H
#define HELLOUTAU_INTERCHANGE_INTERNAL_INTERCHANGECONTRIBUTION_H

#include <helloutau/Widgets/ActionContribution.h>

namespace hello::daw {

    class InterchangeService;

    /// The commands Import and Export > Other Formats in the File menu of the project windows.
    /// Each command opens a wizard for the drivers and the step pages of the service. See
    /// docs/ImportExport.md.
    class InterchangeContribution : public ActionContribution {
    public:
        explicit InterchangeContribution(InterchangeService *service);
        ~InterchangeContribution();

        const QAK::ActionExtension *extension(const QString &windowKind) const override;
        void addActions(QWidget *widget, QAK::WidgetActionContext *context) override;

    private:
        InterchangeService *m_service;
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERNAL_INTERCHANGECONTRIBUTION_H
