#include "UpdateDialog.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    UpdateDialog::UpdateDialog(const QString &version, const QString &currentVersion,
                               const QString &notes, QWidget *parent)
        : QDialog(parent) {
        const auto appName = QApplication::applicationDisplayName();
        setWindowTitle(tr("Update Available"));
        resize(560, 420);

        auto label = new QLabel(tr("%1 %2 is available. The current version is %3.")
                                    .arg(appName, version, currentVersion),
                                this);
        label->setWordWrap(true);

        m_notes = new QTextBrowser(this);
        m_notes->setOpenExternalLinks(true);
        m_notes->setMarkdown(notes);

        auto buttons = new QDialogButtonBox(this);
        const auto download = buttons->addButton(tr("&Download"), QDialogButtonBox::AcceptRole);
        const auto skip =
            buttons->addButton(tr("&Skip This Version"), QDialogButtonBox::RejectRole);
        const auto later = buttons->addButton(tr("Remind Me &Later"), QDialogButtonBox::RejectRole);
        download->setObjectName(QStringLiteral("download"));
        skip->setObjectName(QStringLiteral("skip"));
        later->setObjectName(QStringLiteral("later"));
        download->setDefault(true);
        connect(download, &QPushButton::clicked, this, [this] {
            m_choice = Download;
            accept();
        });
        connect(skip, &QPushButton::clicked, this, [this] {
            m_choice = Skip;
            reject();
        });
        connect(later, &QPushButton::clicked, this, [this] {
            m_choice = Later;
            reject();
        });

        auto layout = new QVBoxLayout(this);
        layout->addWidget(label);
        layout->addWidget(m_notes, 1);
        layout->addWidget(buttons);
    }

    UpdateDialog::~UpdateDialog() = default;

    UpdateDialog::Choice UpdateDialog::choice() const {
        return m_choice;
    }

    QTextBrowser *UpdateDialog::notesBrowser() const {
        return m_notes;
    }

}
