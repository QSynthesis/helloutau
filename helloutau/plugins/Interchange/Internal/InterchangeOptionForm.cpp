#include "InterchangeOptionForm.h"

#include <limits>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>

namespace hello::daw {

    InterchangeOptionForm::InterchangeOptionForm(const QList<kit::InterchangeOption> &options,
                                                 QWidget *parent)
        : QWidget(parent), m_options(options) {
        auto form = new QFormLayout(this);
        for (const auto &option : std::as_const(m_options)) {
            QWidget *control = nullptr;
            switch (option.type) {
                case kit::InterchangeOption::Boolean: {
                    auto box = new QCheckBox(option.name);
                    box->setChecked(option.defaultValue.toBool());
                    connect(box, &QCheckBox::toggled, this, &InterchangeOptionForm::valuesChanged);
                    form->addRow(box);
                    control = box;
                    break;
                }
                case kit::InterchangeOption::Integer: {
                    auto box = new QSpinBox();
                    box->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
                    box->setValue(option.defaultValue.toInt());
                    connect(box, &QSpinBox::valueChanged, this,
                            &InterchangeOptionForm::valuesChanged);
                    form->addRow(option.name, box);
                    control = box;
                    break;
                }
                case kit::InterchangeOption::Choice: {
                    auto box = new QComboBox();
                    box->addItems(option.choices);
                    box->setCurrentIndex(
                        std::max(0, int(option.choices.indexOf(option.defaultValue.toString()))));
                    connect(box, &QComboBox::currentIndexChanged, this,
                            &InterchangeOptionForm::valuesChanged);
                    form->addRow(option.name, box);
                    control = box;
                    break;
                }
                case kit::InterchangeOption::Text: {
                    auto edit = new QLineEdit(option.defaultValue.toString());
                    connect(edit, &QLineEdit::textChanged, this,
                            &InterchangeOptionForm::valuesChanged);
                    form->addRow(option.name, edit);
                    control = edit;
                    break;
                }
            }
            m_controls.push_back(control);
        }
    }

    InterchangeOptionForm::~InterchangeOptionForm() = default;

    QVariantMap InterchangeOptionForm::values() const {
        QVariantMap values;
        for (qsizetype i = 0; i < m_options.size(); ++i) {
            const auto &key = m_options[i].key;
            const auto control = m_controls[i];
            if (const auto box = qobject_cast<QCheckBox *>(control)) {
                values.insert(key, box->isChecked());
            } else if (const auto box = qobject_cast<QSpinBox *>(control)) {
                values.insert(key, box->value());
            } else if (const auto box = qobject_cast<QComboBox *>(control)) {
                values.insert(key, box->currentText());
            } else if (const auto edit = qobject_cast<QLineEdit *>(control)) {
                values.insert(key, edit->text());
            }
        }
        return values;
    }

}
