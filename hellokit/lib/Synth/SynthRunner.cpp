#include "SynthRunner.h"

namespace hello::kit {

    SynthObserver::~SynthObserver() = default;

    void SynthObserver::progressed(int, int) {
    }

    bool SynthObserver::cancelled() {
        return false;
    }

    SynthRunner::SynthRunner() = default;

    SynthRunner::~SynthRunner() = default;

}
