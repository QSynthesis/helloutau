#include "InterchangeService.h"

#include <stdcorelib/pimpl.h>

#include <hellokit/Interchange/InterchangeDrivers.h>

namespace hello::daw {

    namespace {

        InterchangeService *current = nullptr;

    }

    class InterchangeService::Impl {
    public:
        mutable kit::InterchangeDrivers drivers;
        mutable InterchangeStepRegistry stepPages;
    };

    InterchangeService::InterchangeService() : _impl(std::make_unique<Impl>()) {
        if (!current) {
            current = this;
        }
    }

    InterchangeService::~InterchangeService() {
        if (current == this) {
            current = nullptr;
        }
    }

    InterchangeService *InterchangeService::instance() {
        return current;
    }

    kit::InterchangeDrivers &InterchangeService::drivers() const {
        stdc_impl_t;
        return impl.drivers;
    }

    InterchangeStepRegistry &InterchangeService::stepPages() const {
        stdc_impl_t;
        return impl.stepPages;
    }

}
