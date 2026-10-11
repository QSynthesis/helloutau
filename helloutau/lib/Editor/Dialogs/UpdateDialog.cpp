#include "UpdateDialog.h"

#include <QtWidgets/QVBoxLayout>

#include "UpdateTaskView.h"

namespace hello::daw {

    UpdateDialog::UpdateDialog(UpdateTask *task, AppSettings &settings, QWidget *parent)
        : QDialog(parent) {
        setWindowTitle(tr("Updates"));
        resize(560, 420);
        m_view = new UpdateTaskView(task, settings, this);
        connect(m_view, &UpdateTaskView::closeRequested, this, &QDialog::accept);
        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_view);
    }

    UpdateDialog::~UpdateDialog() = default;

    UpdateTaskView *UpdateDialog::view() const {
        return m_view;
    }

}
