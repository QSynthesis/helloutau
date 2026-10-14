#ifndef HELLOUTAU_WIDGETS_FILECHECKLIST_H
#define HELLOUTAU_WIDGETS_FILECHECKLIST_H

#include <functional>

#include <QtCore/QHash>
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
    /// and further columns of the dialog, a CheckAllBox above it, and buttons beside the box. The
    /// buttons check exactly the rows for which a condition returns true, such as the files
    /// without a frequency map.
    ///
    /// The rows are identified by their indices in the order of addRow(). Every row is checkable.
    /// The user resizes the columns with the mouse. The columns are fitted to their contents when
    /// the list is first shown, and no column is stretched to the width of the list.
    class HELLOUTAU_WIDGETS_EXPORT FileCheckList : public QWidget {
        Q_OBJECT
    public:
        /// Creates the list with the columns \a headers , the first of which is the file.
        explicit FileCheckList(const QStringList &headers, QWidget *parent = nullptr);
        ~FileCheckList();

        /// Appends an unchecked row with \a texts , the first of which is the file, and returns
        /// its index.
        int addRow(const QStringList &texts);

        int rowCount() const;

        void setText(int row, int column, const QString &text);

        /// Aligns the header and the cells of \a column by \a alignment , those of the rows
        /// added later included.
        void setColumnAlignment(int column, Qt::Alignment alignment);

        bool isChecked(int row) const;
        void setChecked(int row, bool checked);

        /// Checks the rows for which \a condition returns true and unchecks the others.
        /// checkedChanged() is emitted once.
        void checkWhere(const std::function<bool(int row)> &condition);

        /// Returns the indices of the checked rows in ascending order.
        QList<int> checkedRows() const;

        /// Adds a button with \a text beside the box for all rows. The button checks exactly the
        /// rows for which \a condition returns true, as checkWhere() does.
        QPushButton *addCheckButton(const QString &text, std::function<bool(int row)> condition);

        QTreeWidget *tree() const;
        CheckAllBox *checkAllBox() const;

    Q_SIGNALS:
        /// Emitted when the check state of a row changes, once for a change of checkWhere().
        void checkedChanged();

    protected:
        void showEvent(QShowEvent *event) override;

    private:
        QTreeWidget *m_tree;
        CheckAllBox *m_checkAll;
        QWidget *m_buttons;
        // The alignments of setColumnAlignment() by column, for the rows added later
        QHash<int, Qt::Alignment> m_alignments;
        // Set while checkWhere() changes the rows, so that checkedChanged() is emitted once
        bool m_applying = false;
        // Set once the columns fit their contents, so that a later show keeps the widths that
        // the user set
        bool m_fitted = false;
    };

}

#endif // HELLOUTAU_WIDGETS_FILECHECKLIST_H
