#include "SettingsDialog.h"

#include <QtCore/QDir>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "AppSettings.h"
#include "ExportUstDialog.h"

namespace hello::daw {

    SettingsDialog::SettingsDialog(AppSettings &settings, QWidget *parent)
        : QDialog(parent), m_settings(settings) {
        setWindowTitle(tr("Settings"));

        auto form = new QFormLayout();
        m_utauDirectory =
            addPathRow(form, tr("&UTAU folder:"),
                       QString::fromStdU16String(settings.utauDirectory().u16string()), true);
        form->addRow(new QLabel(tr("Resolves the voice banks of projects that name them "
                                   "relative to UTAU, such as %VOICE%.")));
        m_resampler = addPathRow(form, tr("&Resampler:"), settings.resampler(), false);
        m_wavtool = addPathRow(form, tr("&Wavtool:"), settings.wavtool(), false);

        m_ustExportCharset = new QComboBox();
        m_ustExportCharset->addItems(ExportUstDialog::charsets());
        const int index = m_ustExportCharset->findText(settings.ustExportCharset());
        m_ustExportCharset->setCurrentIndex(index >= 0 ? index : 0);
        form->addRow(tr("UST &export encoding:"), m_ustExportCharset);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
        resize(560, 0);
    }

    SettingsDialog::~SettingsDialog() = default;

    void SettingsDialog::accept() {
        const auto path = [](const QLineEdit *edit) {
            return QDir::fromNativeSeparators(edit->text().trimmed());
        };
        m_settings.setUtauDirectory(std::filesystem::path(path(m_utauDirectory).toStdU16String()));
        m_settings.setResampler(path(m_resampler));
        m_settings.setWavtool(path(m_wavtool));
        m_settings.setUstExportCharset(m_ustExportCharset->currentText());
        QDialog::accept();
    }

    QLineEdit *SettingsDialog::addPathRow(QFormLayout *form, const QString &label,
                                          const QString &text, bool directory) {
        auto edit = new QLineEdit(QDir::toNativeSeparators(text));
        auto button = new QPushButton(tr("Browse..."));
        connect(button, &QPushButton::clicked, this, [this, edit, directory] {
            const auto chosen =
                directory
                    ? QFileDialog::getExistingDirectory(this, tr("Choose Folder"), edit->text())
                    : QFileDialog::getOpenFileName(this, tr("Choose Program"), edit->text());
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

}
