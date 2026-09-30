#ifndef HELLOUTAU_INTERCHANGE_INTERCHANGESTEPPAGE_H
#define HELLOUTAU_INTERCHANGE_INTERCHANGESTEPPAGE_H

#include <QtWidgets/QWidget>

#include <hellokit/Interchange/InterchangeRequest.h>
#include <hellokit/Interchange/InterchangeSource.h>

#include <Interchange/InterchangePluginGlobal.h>

namespace hello::kit {
    class InterchangeReader;
}

namespace hello::daw {

    /// Custom options page of an import driver, which replaces the form generated from the
    /// option schema in the import wizard. See the custom selection steps in
    /// docs/Interchange.md.
    ///
    /// The page sets every option of the schema of the driver. The schema remains the
    /// declaration of the option keys and default values.
    class INTERCHANGEPLUGIN_EXPORT InterchangeStepPage : public QWidget {
        Q_OBJECT
    public:
        explicit InterchangeStepPage(QWidget *parent = nullptr);
        ~InterchangeStepPage() override;

        /// Displays \a source, the inspection result of \a reader, and sets the controls to
        /// their initial values.
        virtual void reset(const kit::InterchangeReader &reader,
                           const kit::InterchangeSource &source) = 0;

        /// Writes the values of the controls into \a request. Returns false if the page is
        /// incomplete.
        virtual bool apply(kit::ImportRequest &request) const = 0;

        /// Returns whether the page is complete. True by default.
        virtual bool isComplete() const;

    Q_SIGNALS:
        /// Emitted when isComplete() may have changed.
        void completeChanged();
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERCHANGESTEPPAGE_H
