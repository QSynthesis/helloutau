#include "ExportUstDialog.h"

#include <QtCore/QDir>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Support/TextCodec.h>

namespace hello::daw {

    ExportUstDialog::ExportUstDialog(const std::filesystem::path &path, const QString &charset,
                                     QWidget *parent)
        : QDialog(parent) {
        setWindowTitle(tr("Export UST"));

        m_path =
            new QLineEdit(QDir::toNativeSeparators(QString::fromStdU16String(path.u16string())));
        auto browseButton = new QPushButton(tr("&Browse..."));
        connect(browseButton, &QPushButton::clicked, this, &ExportUstDialog::browse);
        auto pathRow = new QHBoxLayout();
        pathRow->addWidget(m_path, 1);
        pathRow->addWidget(browseButton);

        m_charset = new QComboBox();
        m_charset->addItems(charsets());
        const int index = m_charset->findText(charset);
        m_charset->setCurrentIndex(index >= 0 ? index : 0);

        auto form = new QFormLayout();
        form->addRow(tr("&File:"), pathRow);
        form->addRow(tr("&Encoding:"), m_charset);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_path, &QLineEdit::textChanged, this, [this, buttons](const QString &text) {
            buttons->button(QDialogButtonBox::Ok)->setEnabled(!text.trimmed().isEmpty());
        });

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
        resize(560, 0);
    }

    ExportUstDialog::~ExportUstDialog() = default;

    std::filesystem::path ExportUstDialog::path() const {
        return std::filesystem::path(
            QDir::fromNativeSeparators(m_path->text().trimmed()).toStdU16String());
    }

    QString ExportUstDialog::charset() const {
        return m_charset->currentText();
    }

    QStringList ExportUstDialog::charsets() {
        QStringList names{QStringLiteral("UTF-8")};
        for (const auto &name : kit::TextCodec::ustCandidates()) {
            if (!names.contains(name)) {
                names.push_back(name);
            }
        }
        return names;
    }

    void ExportUstDialog::browse() {
        const auto file = QFileDialog::getSaveFileName(this, tr("Export UST"), m_path->text(),
                                                       tr("UTAU projects (*.ust)"));
        if (!file.isEmpty()) {
            m_path->setText(QDir::toNativeSeparators(file));
        }
    }

}
