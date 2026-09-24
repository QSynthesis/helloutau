#ifndef HELLOKIT_EDIT_VOICEBANKFIELDS_P_H
#define HELLOKIT_EDIT_VOICEBANKFIELDS_P_H

#include <QtCore/QJsonObject>

#include <hellokit/VoiceBank/VoiceBank.h>

#include <hellokit/EditBase/private/FieldTable_p.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>

namespace hello::kit {

    // The field table of a voice bank tree, one RecordInfo per record type of VoiceBankSchema.h,
    // as ProjectFields_p.h for a project. The field names are the slot names, and the JSON of a
    // record, which commands and logs write, consists of its public fields.

    /// Returns the record of the root of a voice bank tree. The records of all other nodes are
    /// reachable from it through the fields.
    HELLOKIT_EDIT_EXPORT const edit::RecordInfo &voiceBankRecord();

    /// Returns the record of the nodes of type \a nodeType, or \c nullptr if no record of a
    /// voice bank tree has this type.
    HELLOKIT_EDIT_EXPORT const edit::RecordInfo *voiceBankRecordOf(int nodeType);

}

#endif // HELLOKIT_EDIT_VOICEBANKFIELDS_P_H
