#include "SettingPageWidgets_p.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
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
                          const QString &text, bool directory, const QList<QWidget *> &buttons) {
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
        for (const auto extra : buttons) {
            layout->addWidget(extra);
        }
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

    QComboBox *createLogModeBox(bool accumulated) {
        auto box = new QComboBox();
        box->addItem(QCoreApplication::translate("hello::daw::SettingPageWidgets",
                                                 "Keep the latest run only"),
                     false);
        box->addItem(
            QCoreApplication::translate("hello::daw::SettingPageWidgets", "Accumulate runs"), true);
        box->setCurrentIndex(box->findData(accumulated));
        return box;
    }

    QComboBox *createLogLimitBox(int bytes) {
        auto box = new QComboBox();
        box->setEditable(true);
        box->setInsertPolicy(QComboBox::InsertAtBottom);
        const QList<QPair<QString, int>> limits{
            {QStringLiteral("256 KiB"), 256 * 1024      },
            {QStringLiteral("1 MiB"),   1024 * 1024     },
            {QStringLiteral("4 MiB"),   4 * 1024 * 1024 },
            {QStringLiteral("16 MiB"),  16 * 1024 * 1024}
        };
        for (const auto &[label, size] : limits) {
            box->addItem(label, size);
        }
        if (const int index = box->findData(bytes); index >= 0) {
            box->setCurrentIndex(index);
        } else {
            box->setEditText(QString::number(bytes));
        }
        return box;
    }

    std::optional<int> logLimitOf(const QComboBox *box) {
        const auto text = box->currentText();
        if (const int index = box->findText(text, Qt::MatchExactly);
            index >= 0 && box->itemData(index).isValid()) {
            return box->itemData(index).toInt();
        }
        static const QRegularExpression pattern(
            QStringLiteral(R"(^\s*(\d+(?:\.\d+)?)\s*(B|KiB|MiB|GiB)?\s*$)"),
            QRegularExpression::CaseInsensitiveOption);
        const auto match = pattern.match(text);
        if (!match.hasMatch()) {
            return std::nullopt;
        }
        bool ok = false;
        const double number = match.captured(1).toDouble(&ok);
        if (!ok) {
            return std::nullopt;
        }
        const auto suffix = match.captured(2).toLower();
        const double multiplier = suffix == QLatin1String("kib")   ? 1024.0
                                  : suffix == QLatin1String("mib") ? 1024.0 * 1024
                                  : suffix == QLatin1String("gib") ? 1024.0 * 1024 * 1024
                                                                   : 1.0;
        const double size = number * multiplier;
        if (size < 1024 || size > 1024.0 * 1024 * 1024 || size != std::floor(size)) {
            return std::nullopt;
        }
        return int(size);
    }

}
