#include "FolderTree.h"

#include <QtCore/QDir>
#include <QtWidgets/QTreeWidget>

namespace hello::daw {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

    }

    FolderTree::FolderTree(QTreeWidget *tree, Qt::ItemFlags folderFlags)
        : m_tree(tree), m_folderFlags(folderFlags) {
    }

    FolderTree::~FolderTree() = default;

    QTreeWidgetItem *FolderTree::addFile(const std::filesystem::path &path) {
        const bool inside = !path.empty() && !path.has_root_path() && *path.begin() != "..";
        QTreeWidgetItem *parent = nullptr;
        std::filesystem::path folder;
        for (const auto &part : inside ? path.parent_path() : std::filesystem::path()) {
            folder /= part;
            auto &item = m_folders[folder];
            if (!item) {
                item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
                item->setText(0, textOf(part));
                item->setFlags(m_folderFlags);
                if (m_folderFlags.testFlag(Qt::ItemIsUserCheckable)) {
                    item->setCheckState(0, Qt::Unchecked);
                }
                item->setExpanded(true);
            }
            parent = item;
        }
        auto item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        item->setText(0, inside ? textOf(path.filename()) : QDir::toNativeSeparators(textOf(path)));
        return item;
    }

}
