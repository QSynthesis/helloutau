#ifndef HELLOKIT_EDIT_PROJECTFIELDS_P_H
#define HELLOKIT_EDIT_PROJECTFIELDS_P_H

#include <hellokit/Document/Note.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>

#include "FieldTable_p.h"

namespace hello::kit {

    // The field table of a project tree, one RecordInfo per record type of ProjectSchema.h. The
    // table is a set of constants in the source file, and the functions are exported for the
    // tests. The formats of the value types of a project are reached through the fields.

    /// Returns the record of the root of a project tree. The records of all other nodes are
    /// reachable from it through the fields.
    HELLOKIT_EDIT_EXPORT const RecordInfo &projectRecord();

    /// Returns the record of the nodes of type \a nodeType, or \c nullptr if no record of a
    /// project tree has this type.
    HELLOKIT_EDIT_EXPORT const RecordInfo *projectRecordOf(int nodeType);

}

#endif // HELLOKIT_EDIT_PROJECTFIELDS_P_H
