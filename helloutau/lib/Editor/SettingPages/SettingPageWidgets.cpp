#include "SettingPageWidgets_p.h"

#include <algorithm>
#include <limits>
#include <thread>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtGui/QIntValidator>
#include <QtWidgets/QComboBox>
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

    QComboBox *createThreadCountBox(int current) {
        const int hardware = int(std::max(1u, std::thread::hardware_concurrency()));
        auto box = new QComboBox();
        box->setEditable(true);
        box->setInsertPolicy(QComboBox::NoInsert);
        box->setValidator(new QIntValidator(1, std::numeric_limits<int>::max(), box));
        box->addItem(QCoreApplication::translate("hello::daw::SettingPageWidgets", "Automatic (%1)")
                         .arg(hardware),
                     0);
        for (int count = 1; count < hardware; count *= 2) {
            box->addItem(QString::number(count), count);
        }
        box->addItem(QString::number(hardware), hardware);
        if (const int index = box->findData(current); index >= 0) {
            box->setCurrentIndex(index);
        } else {
            box->setEditText(QString::number(current));
        }
        return box;
    }

    std::optional<int> threadCountOf(const QComboBox *box) {
        const auto text = box->currentText();
        if (const int index = box->findText(text); index >= 0) {
            return box->itemData(index).toInt();
        }
        bool number = false;
        const int count = text.trimmed().toInt(&number);
        if (!number || count < 1) {
            return std::nullopt;
        }
        return count;
    }
}
