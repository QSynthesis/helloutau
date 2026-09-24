#include "ProjectValidation_p.h"

#include <substate/VectorNode.h>

#include <hellokit/EditBase/private/EditSession_p.h>
#include <hellokit/EditBase/private/Validation_p.h>

#include "ProjectSession.h"
#include "ProjectTree_p.h"

namespace hello::kit {

    namespace {

        template <class NodeType>
        const NodeType &recordOf(const ss::Node *node) {
            return static_cast<const NodeType &>(*node);
        }

        // The ranges of the slots, one check per slot with a range in ProjectSchema.h.

        void checkSettingsRanges(const SettingsNode &record, QList<edit::Violation> &violations) {
            edit::Validation::checkRange(record, SettingsSlots::Tempo, violations);
        }

        void checkNoteRanges(const NoteNode &record, QList<edit::Violation> &violations) {
            edit::Validation::checkRange(record, NoteSlots::Length, violations);
            edit::Validation::checkRange(record, NoteSlots::NoteNum, violations);
            edit::Validation::checkRange(record, NoteSlots::Tempo, violations);
        }

        // The constraints between fields.

        // UST and .usth hold exactly one track, see docs/UsthFormat.md.
        void checkTrackCount(const ProjectNode &record, QList<edit::Violation> &violations) {
            const auto tracks =
                static_cast<const ss::VectorNode *>(record.child(ProjectSlots::Tracks.index));
            if (tracks->size() != 1) {
                violations.push_back(
                    {ProjectSlots::Tracks.index,
                     ProjectSession::tr("A project contains exactly one track, not %1.")
                         .arg(tracks->size())});
            }
        }

        // The x of each anchor is its distance from the preceding anchor.
        void checkEnvelope(const NoteNode &record, QList<edit::Violation> &violations) {
            const auto envelope = edit::SlotValue<std::optional<Envelope>>::fromVariant(
                record.variant(NoteSlots::Envelope.index));
            if (!envelope) {
                return;
            }
            const auto anchors = envelope->anchorsInTimeOrder();
            for (qsizetype i = 0; i < anchors.size(); ++i) {
                if (anchors[i].x < 0) {
                    violations.push_back(
                        {NoteSlots::Envelope.index,
                         ProjectSession::tr("The envelope anchor %1 is %2 ms before the preceding "
                                            "anchor.")
                             .arg(i + 1)
                             .arg(-anchors[i].x)});
                }
            }
        }

        // The x of each point after the first is its distance from the preceding point. The first
        // point is relative to the start of the note and may precede it.
        void checkPortamento(const NoteNode &record, QList<edit::Violation> &violations) {
            const auto points =
                static_cast<const ss::VectorNode *>(record.child(NoteSlots::Portamento.index));
            for (int i = 1; i < points->size(); ++i) {
                const auto &point = static_cast<const PortamentoPointNode &>(*points->at(i));
                const auto x =
                    edit::SlotValue<double>::fromVariant(point.variant(PortamentoSlots::X.index));
                if (x < 0) {
                    violations.push_back(
                        {NoteSlots::Portamento.index,
                         ProjectSession::tr("The portamento point %1 is %2 ms before the preceding "
                                            "point.")
                             .arg(i + 1)
                             .arg(-x)});
                }
            }
        }

    }

    void registerProjectValidators(edit::EditSession &session) {
        edit::EditSessionPrivate::registerValidator(
            session, ProjectType, [](const ss::Node *node, QList<edit::Violation> &violations) {
                checkTrackCount(recordOf<ProjectNode>(node), violations);
            });
        edit::EditSessionPrivate::registerValidator(
            session, SettingsType, [](const ss::Node *node, QList<edit::Violation> &violations) {
                checkSettingsRanges(recordOf<SettingsNode>(node), violations);
            });
        edit::EditSessionPrivate::registerValidator(
            session, NoteType, [](const ss::Node *node, QList<edit::Violation> &violations) {
                const auto &note = recordOf<NoteNode>(node);
                checkNoteRanges(note, violations);
                checkEnvelope(note, violations);
                checkPortamento(note, violations);
            });
    }

}
