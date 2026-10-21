#include "FormLayout.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QWidget>

namespace hello::daw {

    FormLayout::FormLayout(QWidget *parent) : QFormLayout(parent) {
        setFieldGrowthPolicy(AllNonFixedFieldsGrow);
        setFormAlignment(Qt::AlignLeading | Qt::AlignTop);
        // A negative metric makes the layout ask the style for the spacing between each pair of
        // controls.
        const auto style = parent ? parent->style() : QApplication::style();
        if (style->pixelMetric(QStyle::PM_LayoutVerticalSpacing) < 0) {
            setVerticalSpacing(
                style->layoutSpacing(QSizePolicy::LineEdit, QSizePolicy::LineEdit, Qt::Vertical));
        }
    }

    FormLayout::~FormLayout() = default;

}
