#include "NoteTableModel.h"

#include <QtCore/QTimer>

#include <stdutau/utautils.h>

#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>

namespace hello::daw {

    namespace {

        kit::NoteListRef notesOf(kit::ProjectSession *session) {
            return kit::ProjectRef(session).tracks().at(0).notes();
        }

    }

    NoteTableModel::NoteTableModel(kit::ProjectSession *session, QObject *parent)
        : QAbstractTableModel(parent), m_session(session) {
        m_rows = notesOf(session).size();
        connect(session, &kit::ProjectSession::changed, this, &NoteTableModel::scheduleReset);
    }

    NoteTableModel::~NoteTableModel() = default;

    int NoteTableModel::rowCount(const QModelIndex &parent) const {
        return parent.isValid() ? 0 : m_rows;
    }

    int NoteTableModel::columnCount(const QModelIndex &parent) const {
        return parent.isValid() ? 0 : ColumnCount;
    }

    QVariant NoteTableModel::data(const QModelIndex &index, int role) const {
        const auto notes = notesOf(m_session);
        if (role != Qt::DisplayRole || !index.isValid() || index.row() >= notes.size()) {
            return {};
        }
        const auto note = notes.at(index.row());
        switch (index.column()) {
            case Lyric:
                return note.lyric();
            case Length:
                return note.length();
            case Key:
                return QString::fromStdString(utau::toneNumToToneName(note.noteNum()));
            case Tempo: {
                const auto tempo = note.tempo();
                return tempo ? QVariant(*tempo) : QVariant();
            }
            default:
                break;
        }
        return {};
    }

    QVariant NoteTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
        if (role != Qt::DisplayRole) {
            return {};
        }
        if (orientation == Qt::Vertical) {
            return section;
        }
        switch (section) {
            case Lyric:
                return tr("Lyric");
            case Length:
                return tr("Length");
            case Key:
                return tr("Key");
            case Tempo:
                return tr("Tempo");
            default:
                break;
        }
        return {};
    }

    void NoteTableModel::scheduleReset() {
        if (m_resetPending) {
            return;
        }
        m_resetPending = true;
        QTimer::singleShot(0, this, &NoteTableModel::reset);
    }

    void NoteTableModel::reset() {
        m_resetPending = false;
        beginResetModel();
        m_rows = notesOf(m_session).size();
        endResetModel();
    }

}
