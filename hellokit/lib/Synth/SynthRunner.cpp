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

        /// The number at the start of a fragment name, which identifies the note.
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

    SynthRunner::SynthRunner() : m_outputLog(std::make_shared<SynthToolOutputLog>()) {
    }

    SynthRunner::~SynthRunner() = default;

    std::unique_ptr<SynthToolProcess> SynthRunner::makeSynthToolProcess() const {
        auto synthTool = std::make_unique<SynthToolProcess>(m_outputLog);
        synthTool->timeout = timeout;
        return synthTool;
    }

    void SynthRunner::setOutputLog(std::shared_ptr<SynthToolOutputLog> outputLog) const {
        if (outputLog) {
            m_outputLog = std::move(outputLog);
        }
    }

    std::optional<SynthFailure> SynthRunner::resamplerFailure(const SynthStep &step,
                                                              const SynthTools &synthTools,
                                                              std::optional<SynthToolRun> run) {
        std::error_code error;
        if (fs::exists(step.cacheFile, error)) {
            return std::nullopt;
        }
        SynthFailure failure;
        failure.stage = SynthFailure::Resampler;
        failure.noteIndex = step.noteIndex;
        failure.program = synthTools.resampler;
        failure.arguments = step.resamplerArguments;
        failure.run = std::move(run);
        failure.file = step.cacheFile;
        return failure;
    }

    int SynthRunner::forgetSuperseded(const SynthPlan &plan, DiagnosticList &diagnostics) const {
        Q_UNUSED(diagnostics)

        std::set<int> rendering;
        std::set<fs::path> keeping;
        for (const auto &step : plan.steps()) {
            rendering.insert(step.noteIndex);
            if (!step.cacheFile.empty()) {
                keeping.insert(step.cacheFile);
            }
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
                continue; // the cache file of another note, or of a note this render does not touch
            }
            if (fs::remove(file, error)) {
                ++removed;
            }
        }
        return removed;
    }

}
