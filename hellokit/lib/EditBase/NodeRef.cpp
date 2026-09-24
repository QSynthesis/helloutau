#include "NodeRef.h"

namespace hello::kit::edit {

    bool NodeRef::isValid() const {
        return m_session && m_session->contains(m_id);
    }

}
