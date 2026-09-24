#include "ProjectSession.h"

#include "EditSession_p.h"
#include "ProjectTree_p.h"

namespace hello::kit {

    ProjectSession::ProjectSession(const Project &project, QObject *parent) : EditSession(parent) {
        EditSessionPrivate::setRoot(*this, treeOf(project));
    }

    ProjectSession::~ProjectSession() = default;

    Project ProjectSession::snapshot() const {
        return fromTree<Project>(EditSessionPrivate::find(this, root()));
    }

}
