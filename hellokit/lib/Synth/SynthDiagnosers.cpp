#include "SynthDiagnosers.h"

#include <algorithm>

#include <stdcorelib/pimpl.h>

#include <hellokit/Support/OrderedObjectRegistryKeeper.h>

namespace hello::kit {

    SynthDiagnoser::~SynthDiagnoser() = default;

    int SynthDiagnoser::priority() const {
        return 0;
    }

    class SynthDiagnosers::Impl {
    public:
        OrderedObjectRegistryKeeper<SynthDiagnoser> diagnosers{"synth diagnoser"};
    };

    SynthDiagnosers::SynthDiagnosers() : _impl(std::make_unique<Impl>()) {
    }

    SynthDiagnosers::~SynthDiagnosers() = default;

    SynthDiagnoserRegistry &SynthDiagnosers::registry() const {
        stdc_impl_t;
        return impl.diagnosers.registry();
    }

    QList<SynthDiagnoser *> SynthDiagnosers::diagnosers() const {
        stdc_impl_t;
        auto result = impl.diagnosers.instances();
        std::stable_sort(result.begin(), result.end(), [](SynthDiagnoser *a, SynthDiagnoser *b) {
            return a->priority() > b->priority();
        });
        return result;
    }

    QList<SynthDiagnosis> SynthDiagnosers::diagnose(const SynthDiagnosisContext &context) const {
        QList<SynthDiagnosis> result;
        QList<bool> explained(context.failures.size(), false);
        for (const auto diagnoser : diagnosers()) {
            // The failures not yet explained, and their indexes in context
            SynthDiagnosisContext remaining = context;
            remaining.failures.clear();
            QList<int> indexes;
            for (int i = 0; i < context.failures.size(); ++i) {
                if (!explained.at(i)) {
                    remaining.failures.push_back(context.failures.at(i));
                    indexes.push_back(i);
                }
            }
            for (auto diagnosis : diagnoser->diagnose(remaining)) {
                QList<int> failures;
                for (const auto index : std::as_const(diagnosis.failures)) {
                    if (index >= 0 && index < indexes.size()) {
                        failures.push_back(indexes.at(index));
                        explained[indexes.at(index)] = true;
                    }
                }
                diagnosis.failures = failures;
                result.push_back(diagnosis);
            }
        }
        return result;
    }

}
