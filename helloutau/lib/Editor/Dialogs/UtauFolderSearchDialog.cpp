#include "UtauFolderSearchDialog.h"

#include <QtCore/QDir>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

namespace fs = std::filesystem;

namespace hello::daw {

    namespace {

        // The column of the folder, whose item data holds the path
        constexpr int folderColumn = 0;

    }

    UtauFolderSearchDialog::UtauFolderSearchDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(tr("Search for UTAU Folders"));

        m_list = new QTreeWidget();
        m_list->setRootIsDecorated(false);
        m_list->setHeaderLabels({tr("Folder"), tr("Version"), tr("Found in")});
        // Every column is dragged by the user, and the candidates do not resize them. The last
        // column takes the remaining width.
        m_list->header()->setSectionResizeMode(QHeaderView::Interactive);
        m_list->header()->setStretchLastSection(true);
        m_list->setColumnWidth(folderColumn, 440);
        m_list->setColumnWidth(1, 150);

        m_status = new QLabel();
        m_status->setWordWrap(true);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
        m_use = buttons->addButton(tr("Use This Folder"), QDialogButtonBox::AcceptRole);
        m_use->setEnabled(false);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_list, &QTreeWidget::currentItemChanged, this,
                [this](QTreeWidgetItem *current) { m_use->setEnabled(current != nullptr); });
        connect(m_list, &QTreeWidget::itemDoubleClicked, this, &QDialog::accept);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(tr("The folders that hold utau.exe on this computer. Choose "
                                        "the folder of the UTAU whose synth tools, voice banks "
                                        "and plugins HelloUtau uses.")));
        layout->addWidget(m_list, 1);
        layout->addWidget(m_status);
        layout->addWidget(buttons);
        updateStatus();
        resize(720, 360);
    }

    UtauFolderSearchDialog::~UtauFolderSearchDialog() {
        if (m_thread) {
            m_thread->requestInterruption();
        }
    }

    int UtauFolderSearchDialog::timeout() const {
        return m_timeout;
    }

    void UtauFolderSearchDialog::setTimeout(int milliseconds) {
        m_timeout = milliseconds;
    }

    void UtauFolderSearchDialog::start() {
        // The thread and the search delete themselves once the search has finished, which may
        // be after the dialog is gone. The candidates arrive through queued connections, which
        // end with the dialog.
        auto thread = new QThread();
        auto search = new UtauFolderSearch();
        search->moveToThread(thread);
        connect(thread, &QThread::started, search, &UtauFolderSearch::run);
        connect(search, &UtauFolderSearch::finished, thread, &QThread::quit);
        connect(thread, &QThread::finished, search, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        // A candidate queued after the dialog stopped waiting is dropped on arrival.
        connect(search, &UtauFolderSearch::found, this,
                [this](const UtauFolderSearch::Candidate &candidate) {
                    if (m_searching) {
                        addCandidate(candidate);
                    }
                });
        connect(search, &UtauFolderSearch::finished, this, &UtauFolderSearchDialog::stopWaiting);
        m_thread = thread;
        m_searching = true;
        updateStatus();
        thread->start();
        QTimer::singleShot(m_timeout, this, &UtauFolderSearchDialog::stopWaiting);
    }

    bool UtauFolderSearchDialog::isSearching() const {
        return m_searching;
    }

    void UtauFolderSearchDialog::addCandidate(const UtauFolderSearch::Candidate &candidate) {
        const auto folder = QString::fromStdU16String(candidate.folder.u16string());
        auto item =
            new QTreeWidgetItem(m_list, {QDir::toNativeSeparators(folder), candidate.version,
                                         UtauFolderSearch::sourceName(candidate.source)});
        item->setData(folderColumn, Qt::UserRole, folder);
        item->setToolTip(folderColumn, QDir::toNativeSeparators(folder));
        if (!m_list->currentItem()) {
            m_list->setCurrentItem(item);
        }
        updateStatus();
    }

    std::optional<fs::path> UtauFolderSearchDialog::selectedFolder() const {
        const auto item = m_list->currentItem();
        if (!item) {
            return std::nullopt;
        }
        return fs::path(item->data(folderColumn, Qt::UserRole).toString().toStdU16String());
    }

    QStringList UtauFolderSearchDialog::rows() const {
        QStringList rows;
        for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
            const auto item = m_list->topLevelItem(i);
            rows.push_back(QStringList{item->text(0), item->text(1), item->text(2)}.join(u'\t'));
        }
        return rows;
    }

    QString UtauFolderSearchDialog::statusText() const {
        return m_status->text();
    }

    void UtauFolderSearchDialog::stopWaiting() {
        if (!m_searching) {
            return;
        }
        m_searching = false;
        if (m_thread) {
            m_thread->requestInterruption();
        }
        updateStatus();
    }

    void UtauFolderSearchDialog::updateStatus() {
        const int count = m_list->topLevelItemCount();
        if (m_searching) {
            m_status->setText(tr("Searching..."));
        } else if (count == 0) {
            m_status->setText(tr("No UTAU folder was found. Close this dialog and choose the "
                                 "folder with Browse..."));
        } else {
            m_status->setText(tr("%n folder(s) found.", nullptr, count));
        }
    }

}
