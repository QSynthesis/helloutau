#include <QtTest/QTest>

#include <hellokit/Synth/SynthDiagnosers.h>

using namespace hello::kit;

namespace {

    // Explains the failures whose program has the file name of the diagnoser, and records the
    // number of failures that it received.
    class NamedDiagnoser : public SynthDiagnoser {
    public:
        NamedDiagnoser(QString id, int priority) : m_id(std::move(id)), m_priority(priority) {
        }

        QString id() const override {
            return m_id;
        }

        int priority() const override {
            return m_priority;
        }

        QList<SynthDiagnosis> diagnose(const SynthDiagnosisContext &context) const override {
            received.push_back(context.failures.size());
            SynthDiagnosis diagnosis{m_id, {}, {}, {}};
            for (int i = 0; i < context.failures.size(); ++i) {
                if (QString::fromStdU16String(context.failures.at(i).program.u16string()) == m_id) {
                    diagnosis.failures.push_back(i);
                }
            }
            if (diagnosis.failures.isEmpty()) {
                return {};
            }
            return {diagnosis};
        }

        static inline QList<qsizetype> received;

    private:
        QString m_id;
        int m_priority;
    };

    SynthFailure failureOf(const char *program) {
        SynthFailure failure;
        failure.program = program;
        return failure;
    }

}

class test_SynthDiagnosers : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void diagnosers_run_by_priority_then_registration() {
        SynthDiagnosers diagnosers;
        auto low = SynthDiagnoserRegistry::AddFactory(diagnosers.registry(), "low", {}, [] {
            return std::make_unique<NamedDiagnoser>("low", -5);
        });
        auto first = SynthDiagnoserRegistry::AddFactory(diagnosers.registry(), "first", {}, [] {
            return std::make_unique<NamedDiagnoser>("first", 0);
        });
        auto high = SynthDiagnoserRegistry::AddFactory(diagnosers.registry(), "high", {}, [] {
            return std::make_unique<NamedDiagnoser>("high", 10);
        });
        auto second = SynthDiagnoserRegistry::AddFactory(diagnosers.registry(), "second", {}, [] {
            return std::make_unique<NamedDiagnoser>("second", 0);
        });
        QStringList ids;
        for (const auto diagnoser : diagnosers.diagnosers()) {
            ids.push_back(diagnoser->id());
        }
        QCOMPARE(ids, QStringList({"high", "first", "second", "low"}));
    }

    void a_diagnoser_under_another_name_is_rejected() {
        SynthDiagnosers diagnosers;
        auto wrong = SynthDiagnoserRegistry::AddFactory(diagnosers.registry(), "name", {}, [] {
            return std::make_unique<NamedDiagnoser>("other", 0);
        });
        QVERIFY(diagnosers.diagnosers().isEmpty());
    }

    void an_explained_failure_is_not_passed_on() {
        SynthDiagnosers diagnosers;
        auto high = SynthDiagnoserRegistry::AddFactory(diagnosers.registry(), "b", {}, [] {
            return std::make_unique<NamedDiagnoser>("b", 10);
        });
        auto low = SynthDiagnoserRegistry::AddFactory(diagnosers.registry(), "a", {}, [] {
            return std::make_unique<NamedDiagnoser>("a", 0);
        });
        SynthDiagnosisContext context;
        context.failures = {failureOf("a"), failureOf("b"), failureOf("a"), failureOf("b")};
        NamedDiagnoser::received.clear();
        const auto diagnoses = diagnosers.diagnose(context);
        // b receives four failures and explains two, and a receives the other two.
        QCOMPARE(NamedDiagnoser::received, (QList<qsizetype>{4, 2}));
        QCOMPARE(diagnoses.size(), 2);
        QCOMPARE(diagnoses.at(0).cause, QStringLiteral("b"));
        QCOMPARE(diagnoses.at(0).failures, (QList<int>{1, 3}));
        // The indexes refer to the failures of the context, not to those a received.
        QCOMPARE(diagnoses.at(1).cause, QStringLiteral("a"));
        QCOMPARE(diagnoses.at(1).failures, (QList<int>{0, 2}));
    }

    void an_unregistered_diagnoser_no_longer_runs() {
        SynthDiagnosers diagnosers;
        auto a = SynthDiagnoserRegistry::AddFactory(diagnosers.registry(), "a", {}, [] {
            return std::make_unique<NamedDiagnoser>("a", 0);
        });
        QCOMPARE(diagnosers.diagnosers().size(), 1);
        a = {};
        QVERIFY(diagnosers.diagnosers().isEmpty());
    }
};

QTEST_APPLESS_MAIN(test_SynthDiagnosers)

#include "test_SynthDiagnosers.moc"
