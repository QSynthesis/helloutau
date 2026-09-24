#include "ProjectSession.h"

#include <hellokit/EditBase/private/ChangeLog_p.h>
#include <hellokit/EditBase/private/EditSession_p.h>

#include "ProjectFields_p.h"
#include "ProjectTree_p.h"
#include "ProjectValidation_p.h"

namespace hello::kit {

    ProjectSession::ProjectSession(const Project &project, QObject *parent)
        : edit::EditSession(parent) {
        edit::EditSessionPrivate::setRoot(*this, treeOf(project));
        registerProjectValidators(*this);
    }

    ProjectSession::~ProjectSession() = default;

    Project ProjectSession::snapshot() const {
        return edit::fromTree<Project>(edit::EditSessionPrivate::find(this, root()));
    }

    std::optional<QJsonObject> ProjectSession::logEntry(const edit::Change &change) const {
        return edit::ChangeLog::entryOf(*this, change, projectRecordOf);
    }

}
