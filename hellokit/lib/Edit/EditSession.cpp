#include "EditSession.h"

#include <substate/Model.h>

#include "ProjectTree_p.h"

namespace hello::kit {

    class EditSession::Impl {
    public:
        ss::Model model;
    };

    EditSession::EditSession(const Project &project) : _impl(std::make_unique<Impl>()) {
        _impl->model.reset(treeOf(project));
    }

    EditSession::~EditSession() = default;

    Project EditSession::snapshot() const {
        return projectOf(_impl->model.root());
    }

}
