#include "Backend.h"

namespace fswatcher {

    // Nothing follows anything here yet. Saying so is the honest answer, and the other side
    // falls back to looking at the disk itself.
    class Backend::Impl {
    public:
        explicit Impl(Output &out) : out(out) {
        }

        Output &out;
    };

    void prepareProcess() {
        // Streams have no text mode here, and nothing puts up a dialog.
    }

    Backend::Backend(Output &out) : m_impl(std::make_unique<Impl>(out)) {
    }

    Backend::~Backend() = default;

    void Backend::follow(const std::vector<std::string> &roots) {
        for (const auto &root : roots) {
            m_impl->out.line("unwatchable", root);
        }
        m_impl->out.line("ok");
    }

}
