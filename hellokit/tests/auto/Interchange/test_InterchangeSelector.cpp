#include <algorithm>

#include <QtTest/QTest>

#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeSelector.h>

using namespace hello::kit;

namespace {

    // Only what AutomaticSelector asks of a driver, which is the list of options it declared.
    class SchemaOnlyReader : public InterchangeReader {
    public:
        QString id() const override {
            return QStringLiteral("fake");
        }
        QString name() const override {
            return QStringLiteral("Fake");
        }
        QStringList suffixes() const override {
            return {QStringLiteral("fake")};
        }

        QList<InterchangeOption> optionSchema() const override {
            InterchangeOption option;
            option.key = QStringLiteral("encoding");
            option.name = QStringLiteral("Encoding");
            option.type = InterchangeOption::Choice;
            option.defaultValue = QStringLiteral("UTF-8");
            option.choices = {QStringLiteral("UTF-8"), QStringLiteral("Shift_JIS")};
            return {option};
        }

        std::optional<InterchangeSource> inspect(const std::filesystem::path &,
                                                 DiagnosticList &) override {
            return std::nullopt;
        }

    protected:
        std::optional<Project> convert(const std::filesystem::path &, const InterchangeSource &,
                                       const ImportRequest &, DiagnosticList &) override {
            return std::nullopt;
        }
    };

    InterchangeSource sourceWith(int entryCount) {
        InterchangeSource source;
        source.formatId = QStringLiteral("fake");
        for (int i = 0; i < entryCount; ++i) {
            InterchangeEntry entry;
            entry.index = i;
            entry.noteCount = 1;
            source.entries.push_back(entry);
        }
        return source;
    }

    bool warned(const DiagnosticList &diagnostics) {
        return std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic &d) {
            return d.severity == DiagnosticSeverity::Warning;
        });
    }

}

class test_InterchangeSelector : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The command line and the tests have nobody to ask, so there has to be an answer available
    // without a user. It comes from what the driver declared.
    void it_takes_the_declared_defaults() {
        SchemaOnlyReader reader;
        AutomaticSelector selector;
        DiagnosticList diagnostics;

        const auto request = selector.selectImport(reader, sourceWith(1), {}, diagnostics);
        QVERIFY(request.has_value());
        QCOMPARE(request->driverOptions.value("encoding").toString(), QStringLiteral("UTF-8"));
    }

    // A project holds one track, so three entries cannot all come in. Which ones were left is
    // something the caller can only learn from the diagnostics.
    void it_obeys_the_limit_and_says_so() {
        SchemaOnlyReader reader;
        AutomaticSelector selector;
        DiagnosticList diagnostics;

        const auto request = selector.selectImport(reader, sourceWith(3), {}, diagnostics);
        QVERIFY(request.has_value());
        QCOMPARE(request->entries.size(), 1);
        QVERIFY(warned(diagnostics));
    }

    void a_raised_limit_takes_more() {
        SchemaOnlyReader reader;
        AutomaticSelector selector;
        DiagnosticList diagnostics;

        ImportLimits limits;
        limits.maxEntries = 3;
        const auto request = selector.selectImport(reader, sourceWith(3), limits, diagnostics);
        QVERIFY(request.has_value());
        QCOMPARE(request->entries.size(), 3);
    }

    // Returning nothing means two different things, and only the diagnostics tell them apart.
    // Giving up without saying so arrives at the caller looking like a cancellation.
    void nothing_to_choose_from_is_an_error_not_a_cancellation() {
        SchemaOnlyReader reader;
        AutomaticSelector selector;
        DiagnosticList diagnostics;

        const auto request = selector.selectImport(reader, sourceWith(0), {}, diagnostics);
        QVERIFY(!request.has_value());
        QVERIFY(hasError(diagnostics));
    }
};

QTEST_APPLESS_MAIN(test_InterchangeSelector)

#include "test_InterchangeSelector.moc"
