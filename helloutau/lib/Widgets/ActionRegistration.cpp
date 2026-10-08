#include "ActionRegistration.h"

#include <utility>

namespace hello::daw {

    ActionRegistration::ActionRegistration(ActionContributionRegistry *registry,
                                           std::unique_ptr<ActionContribution> contribution)
        : m_registry(registry), m_contribution(std::move(contribution)) {
        if (m_registry) {
            m_registry->add(m_contribution.get());
        }
    }

    ActionRegistration::~ActionRegistration() {
        if (m_registry) {
            m_registry->remove(m_contribution.get());
        }
    }

    ActionContribution *ActionRegistration::contribution() const {
        return m_contribution.get();
    }

}
