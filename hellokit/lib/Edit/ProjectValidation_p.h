#ifndef HELLOKIT_EDIT_PROJECTVALIDATION_P_H
#define HELLOKIT_EDIT_PROJECTVALIDATION_P_H

#include <hellokit/EditBase/EditSession.h>

namespace hello::kit {

    /// Registers the validators of the records of a project tree with \a session: the ranges of
    /// the slots declared in ProjectSchema.h, and the constraints between fields.
    void registerProjectValidators(edit::EditSession &session);

}

#endif // HELLOKIT_EDIT_PROJECTVALIDATION_P_H
