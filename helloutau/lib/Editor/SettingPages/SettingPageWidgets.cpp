#include "SettingPageWidgets_p.h"

#include <QtCore/QDir>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

#include <helloutau/Widgets/SettingPage.h>

namespace hello::daw {

    QString pathText(const QLineEdit *edit) {
        return QDir::fromNativeSeparators(edit->text().trimmed());
    }

    QLineEdit *addPathRow(QFormLayout *form, QWidget *owner, const QString &label,
                          const QString &text, bool directory) {
        auto edit = new QLineEdit(QDir::toNativeSeparators(text));
        auto button = new QPushButton(SettingPage::tr("Browse..."));
        QObject::connect(button, &QPushButton::clicked, owner, [owner, edit, directory] {
            const auto chosen =
                directory
                    ? QFileDialog::getExistingDirectory(owner, SettingPage::tr("Choose Folder"),
                                                        edit->text())
                    : QFileDialog::getOpenFileName(owner, SettingPage::tr("Choose Program"),
                                                   edit->text());
            if (!chosen.isEmpty()) {
                edit->setText(QDir::toNativeSeparators(chosen));
            }
        });
        auto layout = new QHBoxLayout();
        layout->addWidget(edit, 1);
        layout->addWidget(button);
        form->addRow(label, layout);
        return edit;
    }

    QLabel *note(const QString &text) {
        auto label = new QLabel(text);
        label->setWordWrap(true);
        return label;
    }

}
