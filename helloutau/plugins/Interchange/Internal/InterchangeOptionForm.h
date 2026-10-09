#ifndef HELLOUTAU_INTERCHANGE_INTERNAL_INTERCHANGEOPTIONFORM_H
#define HELLOUTAU_INTERCHANGE_INTERNAL_INTERCHANGEOPTIONFORM_H

#include <QtCore/QList>
#include <QtCore/QVariantMap>
#include <QtWidgets/QWidget>

#include <hellokit/Interchange/InterchangeRequest.h>

namespace hello::daw {

    /// A form generated from the option schema of a driver, with one control per option,
    /// initialized to the default value of the option.
    class InterchangeOptionForm : public QWidget {
        Q_OBJECT
    public:
        explicit InterchangeOptionForm(const QList<kit::InterchangeOption> &options,
                                       QWidget *parent = nullptr);
        ~InterchangeOptionForm() override;

        /// Returns the values of the controls, keyed by option key.
        QVariantMap values() const;

    Q_SIGNALS:
        void valuesChanged();

    private:
        QList<kit::InterchangeOption> m_options;
        QList<QWidget *> m_controls;
    };

}

#endif // HELLOUTAU_INTERCHANGE_INTERNAL_INTERCHANGEOPTIONFORM_H
