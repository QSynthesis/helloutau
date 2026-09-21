#include "SynthRunner.h"

#include <memory>
#include <optional>
#include <set>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        /// The number a piece's name starts with, which is the note it belongs to.
        std::optional<int> noteOf(const fs::path &file) {
            const auto name = QString::fromStdU16String(file.filename().u16string());
            const int end = name.indexOf(QLatin1Char('_'));
            if (end <= 0) {
                return std::nullopt;
            }
            bool ok = false;
            const int index = name.left(end).toInt(&ok);
            return ok ? std::optional<int>(index) : std::nullopt;
        }

    }

    SynthObserver::~SynthObserver() = default;

    void SynthObserver::progressed(int, int) {
    }

    bool SynthObserver::cancelled() {
        return false;
    }

    SynthRunner::SynthRunner() = default;

    SynthRunner::~SynthRunner() = default;

    std::unique_ptr<EngineProcess> SynthRunner::makeEngineProcess() const {
        auto engine = std::make_unique<EngineProcess>();
        engine->timeout = timeout;
        return engine;
    }

    int SynthRunner::forgetSuperseded(const SynthPlan &plan, DiagnosticList &diagnostics) const {
        Q_UNUSED(diagnostics)

        std::set<int> rendering;
        std::set<fs::path> keeping;
        for (const auto &step : plan.steps()) {
            rendering.insert(step.noteIndex);
            keeping.insert(step.cacheFile);
        }

        std::error_code error;
        fs::directory_iterator here(plan.cacheDirectory(), error);
        if (error) {
            return 0;
        }

        int removed = 0;
        for (const auto &entry : here) {
            const auto &file = entry.path();
            if (keeping.count(file)) {
                continue;
            }
            const auto note = noteOf(file);
            if (!note || !rendering.count(*note)) {
                continue; // somebody else's, or a note this render is not touching
            }
            if (fs::remove(file, error)) {
                ++removed;
            }
        }
        return removed;
    }

}
