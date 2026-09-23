#include <algorithm>

#include <QtTest/QTest>

#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeSelector.h>

using namespace hello::kit;

namespace {

    // Only the property AutomaticSelector uses, which is the list of declared options.
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
    // The command line and the tests have no user to ask, so the selector must decide without
    // one. The decision follows the defaults declared by the driver.
    void it_takes_the_declared_defaults() {
        SchemaOnlyReader reader;
        AutomaticSelector selector;
        DiagnosticList diagnostics;

        const auto request = selector.selectImport(reader, sourceWith(1), {}, diagnostics);
        QVERIFY(request.has_value());
        QCOMPARE(request->driverOptions.value("encoding").toString(), QStringLiteral("UTF-8"));
    }

    // A project holds one track, so three entries cannot all be imported. The caller can learn
    // which entries were omitted only from the diagnostics.
    void the_track_limit_is_applied_with_a_warning() {
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

    // An empty result has two meanings, distinguished only by the diagnostics. A failure
    // without a recorded error reaches the caller as a cancellation.
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
