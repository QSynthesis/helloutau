#ifndef HELLOKIT_EDIT_VOICEBANKVALIDATION_P_H
#define HELLOKIT_EDIT_VOICEBANKVALIDATION_P_H

#include <hellokit/EditBase/EditSession.h>

namespace hello::kit {

    /// Registers the validators of the records of a voice bank tree with \a session: the
    /// constraints of the entries and of the prefix map. See the section on the voice bank in
    /// docs/Editing.md.
    void registerVoiceBankValidators(edit::EditSession &session);

}

#endif // HELLOKIT_EDIT_VOICEBANKVALIDATION_P_H
