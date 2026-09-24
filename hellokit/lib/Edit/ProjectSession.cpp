#include "ProjectSession.h"

#include <vector>

#include "EditSession_p.h"
#include "ProjectTree_p.h"

namespace hello::kit {

    namespace {

        // Inserts the trees of values into the list of id, whose items belong to a record of
        // type owner. Items of another kind in a list are a programming error.
        template <class Value, class Convert>
        void insertItems(ProjectSession &session, NodeId list, int owner, int index,
                         const QList<Value> &values, Convert convert) {
            const auto node = EditSessionPrivate::find<ss::VectorNode>(session, list);
            if (node && node->parent()->type() != owner) {
                Q_ASSERT_X(false, "ProjectSession", "an insertion of items of another kind");
                return;
            }
            std::vector<std::unique_ptr<ss::Node>> items;
            items.reserve(size_t(values.size()));
            for (const auto &value : values) {
                items.push_back(convert(value));
            }
            EditSessionPrivate::insert(session, list, index, std::move(items));
        }

    }

    ProjectSession::ProjectSession(const Project &project, QObject *parent) : EditSession(parent) {
        EditSessionPrivate::setRoot(*this, treeOf(project));
    }

    ProjectSession::~ProjectSession() = default;

    Project ProjectSession::snapshot() const {
        return projectOf(EditSessionPrivate::find(*this, root()));
    }

    Note ProjectSession::note(NodeId note) const {
        const auto node = EditSessionPrivate::find(*this, note);
        return node && node->type() == NoteType ? noteOfTree(node) : Note();
    }

    void ProjectSession::insert(NodeId list, int index, const QList<Note> &notes) {
        insertItems(*this, list, TrackType, index, notes, treeOfNote);
    }

    void ProjectSession::insert(NodeId list, int index, const QList<PortamentoPoint> &points) {
        insertItems(*this, list, NoteType, index, points, treeOfPortamentoPoint);
    }

    void ProjectSession::setPitchBend(NodeId note, const std::optional<PitchBend> &pitchBend) {
        const auto node = EditSessionPrivate::find(*this, note);
        Q_ASSERT(!node || node->type() == NoteType);
        if (!pitchBend) {
            removeChild(note, NoteSlots::PitchBend);
            return;
        }
        EditSessionPrivate::setChild(*this, note, NoteSlots::PitchBend.index,
                                     treeOfPitchBend(*pitchBend));
    }

}
