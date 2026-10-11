#include "SynthDiagnosers.h"

#include <algorithm>

#include <QtCore/QtDebug>

#include <stdcorelib/pimpl.h>

#include <hellokit/Support/RegistryInstanceList.h>

namespace hello::kit {

    SynthDiagnoser::~SynthDiagnoser() = default;

    int SynthDiagnoser::priority() const {
        return 0;
    }

    class SynthDiagnosers::Impl {
    public:
        using Instances = RegistryInstanceList<SynthDiagnoserRegistry>;

        Impl()
            : instances(registry, [](Instances::Item &item) {
                  if (!item.instance) {
                      return false;
                  }
                  const auto name = QString::fromStdString(item.entry->name());
                  if (item.instance->id() != name) {
                      qWarning("The synth diagnoser \"%s\" is registered under the name \"%s\".",
                               qUtf8Printable(item.instance->id()), qUtf8Printable(name));
                      return false;
                  }
                  return true;
              }) {
        }

        // The instances are declared after their registry, so that they are destroyed first.
        mutable SynthDiagnoserRegistry registry;
        Instances instances;
    };

    SynthDiagnosers::SynthDiagnosers() : _impl(std::make_unique<Impl>()) {
    }

    SynthDiagnosers::~SynthDiagnosers() = default;

    SynthDiagnoserRegistry &SynthDiagnosers::registry() const {
        stdc_impl_t;
        return impl.registry;
    }

    QList<SynthDiagnoser *> SynthDiagnosers::diagnosers() const {
        stdc_impl_t;
        QList<SynthDiagnoser *> result;
        for (const auto &item : impl.instances.items()) {
            result.push_back(item.instance.get());
        }
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
