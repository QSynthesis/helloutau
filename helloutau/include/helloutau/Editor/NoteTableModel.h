#ifndef HELLOUTAU_EDITOR_NOTETABLEMODEL_H
#define HELLOUTAU_EDITOR_NOTETABLEMODEL_H

#include <QtCore/QAbstractTableModel>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::kit {
    class ProjectSession;
}

namespace hello::daw {

    /// The notes of the first track of a session as a read-only table, until the piano roll
    /// replaces it, see docs/Widgets.md.
    ///
    /// The model reads the tree of the session. Changes are not applied one by one: a change
    /// only schedules a reset, which happens once control returns to the event loop, so that a
    /// transaction of many changes resets the model once.
    class HELLOUTAU_EDITOR_EXPORT NoteTableModel : public QAbstractTableModel {
        Q_OBJECT
    public:
        enum Column {
            Lyric,
            Length,
            Key,
            Tempo,
            ColumnCount,
        };

        explicit NoteTableModel(kit::ProjectSession *session, QObject *parent = nullptr);
        ~NoteTableModel();

        int rowCount(const QModelIndex &parent = {}) const override;
        int columnCount(const QModelIndex &parent = {}) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        QVariant headerData(int section, Qt::Orientation orientation,
                            int role = Qt::DisplayRole) const override;

    private:
        kit::ProjectSession *m_session;

        // The number of rows at the last reset. The tree may already hold another number while
        // a reset is pending, and the model must not report a count it has not announced.
        int m_rows = 0;
        bool m_resetPending = false;

        void scheduleReset();
        void reset();
    };

}

#endif // HELLOUTAU_EDITOR_NOTETABLEMODEL_H
