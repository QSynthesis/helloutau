#ifndef HELLOUTAU_WIDGETS_FILECHECKLIST_H
#define HELLOUTAU_WIDGETS_FILECHECKLIST_H

#include <functional>

#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QPushButton;
class QTreeWidget;

namespace hello::daw {

    class CheckAllBox;

    /// The list of files of a dialog that acts on the files that the user checks, such as the
    /// audio files of a voice bank to process: a list of rows with the file in the first column
    /// and further columns of the dialog, a CheckAllBox above it, and buttons beside the box that
    /// check exactly the rows of a condition, such as the files without a frequency map.
    ///
    /// The rows are identified by their indices in the order of addRow(). Every row is checkable.
    class HELLOUTAU_WIDGETS_EXPORT FileCheckList : public QWidget {
        Q_OBJECT
    public:
        /// Creates the list with the column \a headers , the first of which is the file.
        explicit FileCheckList(const QStringList &headers, QWidget *parent = nullptr);
        ~FileCheckList();

        /// Appends an unchecked row with \a texts , the first of which is the file, and returns
        /// its index.
        int addRow(const QStringList &texts);

        int rowCount() const;

        void setText(int row, int column, const QString &text);

        bool isChecked(int row) const;
        void setChecked(int row, bool checked);

        /// Checks the rows for which \a condition returns true and unchecks the others.
        /// checkedChanged() is emitted once.
        void checkWhere(const std::function<bool(int row)> &condition);

        /// Returns the indices of the checked rows in ascending order.
        QList<int> checkedRows() const;

        /// Adds a button with \a text beside the box for all rows that checks exactly the rows
        /// for which \a condition returns true, as checkWhere() does.
        QPushButton *addCheckButton(const QString &text, std::function<bool(int row)> condition);

        QTreeWidget *tree() const;
        CheckAllBox *checkAllBox() const;

    Q_SIGNALS:
        /// Emitted when the check state of a row changes, once for a change of checkWhere().
        void checkedChanged();

    private:
        QTreeWidget *m_tree;
        CheckAllBox *m_checkAll;
        QWidget *m_buttons;
        // Set while checkWhere() changes the rows, so that checkedChanged() is emitted once
        bool m_applying = false;
    };

}

#endif // HELLOUTAU_WIDGETS_FILECHECKLIST_H
