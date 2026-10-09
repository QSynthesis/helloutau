#ifndef HELLOKIT_EDITBASE_CHANGELOG_P_H
#define HELLOKIT_EDITBASE_CHANGELOG_P_H

#include <optional>

#include <QtCore/QJsonObject>

#include <hellokit/EditBase/Change.h>
#include <hellokit/EditBase/EditSession.h>
#include <hellokit/EditBase/HelloKitEditBaseGlobal.h>

#include "EditSession_p.h"

namespace hello::kit::edit {

    /// The entries of the change log, one JSON object per change. See the section on the change
    /// log in docs/Editing.md.
    ///
    /// The entry of each kind of change is written by the LogWriter registered for the kind with
    /// EditSessionPrivate::registerLogWriter(). The kinds of this library are registered through
    /// the same interface as the kinds of node types added later.
    struct HELLOKIT_EDITBASE_EXPORT ChangeLog {
        /// Returns the entry of \a change in \a session, or \c std::nullopt if the writer of its
        /// kind omits it. The entry of a kind without a writer records the kind and the node.
        ///
        /// A writer names the slots and writes the values by the field table, which \a lookup
        /// provides. The entry is therefore created while the change is reported, when the
        /// changed node is in the tree.
        static std::optional<QJsonObject> entryOf(const EditSession &session, const Change &change,
                                                  RecordLookup lookup);

        /// Registers the writers of the kinds of changes of this library with \a session.
        ///
        /// The entry of a slot names the slot and writes its values by the field table. The slot
        /// of a record without a field table, or of a node no longer in the tree, is written as
        /// its index, and its values and the values of mapping entries as
        /// QJsonValue::fromVariant() converts them.
        static void registerBuiltInWriters(EditSession &session);
    };

}

#endif // HELLOKIT_EDITBASE_CHANGELOG_P_H
