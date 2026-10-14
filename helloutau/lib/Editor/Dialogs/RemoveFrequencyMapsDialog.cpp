#include "RemoveFrequencyMapsDialog.h"

#include <QtCore/QDir>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <helloutau/Widgets/FileCheckList.h>

namespace hello::daw {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

    }

    RemoveFrequencyMapsDialog::RemoveFrequencyMapsDialog(QList<File> files,
                                                         const std::filesystem::path &root,
                                                         const QString &formatName,
                                                         const QStringList &cacheNames,
                                                         QWidget *parent)
        : QDialog(parent), m_files(std::move(files)) {
        setWindowTitle(tr("Remove Frequency Maps"));

        auto format = new QLabel(tr("The frequency maps of the format %1 and their analysis "
                                    "caches are removed for the checked audio files.")
                                     .arg(formatName));
        format->setWordWrap(true);

        // A check mark for a file present, nothing for a file missing, as in the table of the
        // window
        const auto presence = [](bool present) {
            return present ? QString(QChar(0x2713)) : QString();
        };

        // The rows are in the order of the files, and the columns of the caches follow the
        // column of the map.
        m_list = new FileCheckList(QStringList{tr("File"), tr("Frequency Map")} + cacheNames);
        for (int column = 1; column < 2 + cacheNames.size(); ++column) {
            m_list->setColumnAlignment(column, Qt::AlignCenter);
        }
        for (const auto &file : std::as_const(m_files)) {
            const auto relative = file.path.lexically_relative(root);
            QStringList texts{
                QDir::toNativeSeparators(textOf(relative.empty() ? file.path : relative)),
                presence(file.hasMap)};
            for (qsizetype i = 0; i < cacheNames.size(); ++i) {
                texts.push_back(presence(file.caches.value(i)));
            }
            m_list->addRow(texts);
        }
        m_checkMaps = m_list->addCheckButton(tr("Check Files with &Maps"),
                                             [this](int row) { return m_files.at(row).hasMap; });
        // No file is checked without a selection, because the removal cannot be undone.
        m_list->checkWhere([this](int row) { return m_files.at(row).selected; });

        m_summary = new QLabel();
        m_summary->setWordWrap(true);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
        m_remove = buttons->addButton(tr("&Remove"), QDialogButtonBox::AcceptRole);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(format);
        layout->addWidget(m_list, 1);
        layout->addWidget(m_summary);
        layout->addWidget(buttons);
        resize(640, 560);

        connect(m_list, &FileCheckList::checkedChanged, this,
                &RemoveFrequencyMapsDialog::updateSummary);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        updateSummary();
    }

    RemoveFrequencyMapsDialog::~RemoveFrequencyMapsDialog() = default;

    QList<std::filesystem::path> RemoveFrequencyMapsDialog::checkedFiles() const {
        QList<std::filesystem::path> files;
        for (const int row : m_list->checkedRows()) {
            files.push_back(m_files.at(row).path);
        }
        return files;
    }

    FileCheckList *RemoveFrequencyMapsDialog::fileList() const {
        return m_list;
    }

    QPushButton *RemoveFrequencyMapsDialog::checkMapsButton() const {
        return m_checkMaps;
    }

    QPushButton *RemoveFrequencyMapsDialog::removeButton() const {
        return m_remove;
    }

    void RemoveFrequencyMapsDialog::updateSummary() {
        const auto count = int(m_list->checkedRows().size());
        m_summary->setText(count > 0 ? tr("The frequency maps and the analysis caches of %n audio "
                                          "file(s) will be removed. The removal cannot be "
                                          "undone.",
                                          nullptr, count)
                                     : tr("No file is checked."));
        m_remove->setEnabled(count > 0);
    }

}
