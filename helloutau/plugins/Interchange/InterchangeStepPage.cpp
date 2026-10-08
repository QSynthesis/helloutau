#include "InterchangeStepPage.h"

namespace hello::daw {

    InterchangeStepPage::InterchangeStepPage(QWidget *parent) : QWidget(parent) {
    }

    InterchangeStepPage::~InterchangeStepPage() = default;

    bool InterchangeStepPage::isComplete() const {
        return true;
    }

}
