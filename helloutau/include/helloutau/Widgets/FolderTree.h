#ifndef HELLOUTAU_WIDGETS_FOLDERTREE_H
#define HELLOUTAU_WIDGETS_FOLDERTREE_H

#include <filesystem>
#include <map>

#include <QtCore/Qt>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QTreeWidget;
class QTreeWidgetItem;

namespace hello::daw {

    /// The items of the folders of a QTreeWidget that lists files in a tree of their folders,
    /// such as the audio files of a voice bank. The item of a file lies under the items of its
    /// folders, which are created as needed in the order of their first files. The item of a
    /// folder has the folder name in the first column, is expanded, and has the flags given to
    /// the constructor.
    class HELLOUTAU_WIDGETS_EXPORT FolderTree {
    public:
        /// Places the items of the files and the folders in \a tree. The items of the folders have
        /// \a folderFlags. If these include Qt::ItemIsUserCheckable, the items of the folders are
        /// created unchecked.
        explicit FolderTree(QTreeWidget *tree, Qt::ItemFlags folderFlags = Qt::ItemIsEnabled);
        ~FolderTree();

        /// Creates the item of the file at \a path, which is relative to the folder that the tree
        /// lists, under the items of its folders, with the file name in the first column. If
        /// \a path is empty, has a root or begins with \c .., the item lies at the top level with
        /// the whole path in native separators.
        QTreeWidgetItem *addFile(const std::filesystem::path &path);

    private:
        QTreeWidget *m_tree;
        Qt::ItemFlags m_folderFlags;
        // The items of the folders by their paths relative to the folder that the tree lists
        std::map<std::filesystem::path, QTreeWidgetItem *> m_folders;
    };

}

#endif // HELLOUTAU_WIDGETS_FOLDERTREE_H
