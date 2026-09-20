#include "InterchangePlugin.h"

namespace hello::kit {

    InterchangePlugin::~InterchangePlugin() = default;

    std::vector<std::unique_ptr<InterchangeReader>> InterchangePlugin::createReaders() {
        return {};
    }

    std::vector<std::unique_ptr<InterchangeWriter>> InterchangePlugin::createWriters() {
        return {};
    }

}
