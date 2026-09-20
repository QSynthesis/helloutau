#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE test_Interchange

#include <boost/test/unit_test.hpp>

#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeRegistry.h>
#include <hellokit/Interchange/InterchangeSelector.h>

using namespace hello::kit;

namespace {

    // A driver with nothing behind it, so that the flow read() runs can be checked without a
    // file format in the way. It records what it was asked to convert.
    class FakeReader : public InterchangeReader {
    public:
        QString id() const override {
            return QStringLiteral("fake");
        }
        QString name() const override {
            return QStringLiteral("Fake");
        }
        QStringList suffixes() const override {
            return {QStringLiteral("fake"), QStringLiteral("FK")};
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

        std::optional<InterchangeSource> inspect(const std::filesystem::path &path,
                                                 DiagnosticList &diagnostics) override {
            Q_UNUSED(path)
            if (!readable) {
                diagnostics.push_back({DiagnosticSeverity::Error, QStringLiteral("no")});
                return std::nullopt;
            }
            InterchangeSource source;
            source.formatId = id();
            for (int i = 0; i < entryCount; ++i) {
                InterchangeEntry entry;
                entry.index = i;
                entry.noteCount = 1;
                entry.rawName = QByteArray("track") + QByteArray::number(i);
                source.entries.push_back(entry);
            }
            return source;
        }

        bool readable = true;
        int entryCount = 3;
        mutable ImportRequest seen;

    protected:
        std::optional<Project> convert(const std::filesystem::path &path,
                                       const InterchangeSource &source,
                                       const ImportRequest &request,
                                       DiagnosticList &diagnostics) override {
            Q_UNUSED(path)
            Q_UNUSED(source)
            Q_UNUSED(diagnostics)
            seen = request;
            Project project;
            project.tracks.push_back({});
            return project;
        }
    };

    class CancellingSelector : public InterchangeSelector {
    public:
        std::optional<ImportRequest> selectImport(const InterchangeReader &, //
                                                  const InterchangeSource &,
                                                  const ImportLimits &,
                                                  DiagnosticList &) override {
            return std::nullopt;
        }
        std::optional<ExportRequest> selectExport(const InterchangeWriter &, const Project &,
                                                  DiagnosticList &) override {
            return std::nullopt;
        }
    };

}

BOOST_AUTO_TEST_SUITE(test_Interchange)

// A driver handed no selector still has to finish, since the command line and the tests have
// nobody to ask. Defaults come from the schema the driver declared.
BOOST_AUTO_TEST_CASE(automatic_selector_takes_declared_defaults) {
    FakeReader reader;
    auto result = reader.read("whatever.fake", nullptr);

    BOOST_REQUIRE(result.project.has_value());
    BOOST_CHECK(!result.cancelled);
    BOOST_CHECK_EQUAL(reader.seen.driverOptions.value("encoding").toString().toStdString(),
                      "UTF-8");
}

// The project holds one track, so three entries cannot all come in. Which ones were left is
// something the caller can only learn from the diagnostics.
BOOST_AUTO_TEST_CASE(automatic_selector_obeys_the_limit_and_says_so) {
    FakeReader reader;
    auto result = reader.read("whatever.fake", nullptr);

    BOOST_REQUIRE(result.project.has_value());
    BOOST_CHECK_EQUAL(reader.seen.entries.size(), 1);

    const bool warned = std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                                    [](const Diagnostic &d) {
                                        return d.severity == DiagnosticSeverity::Warning;
                                    });
    BOOST_CHECK(warned);
}

BOOST_AUTO_TEST_CASE(a_raised_limit_takes_more) {
    FakeReader reader;
    ImportLimits limits;
    limits.maxEntries = 3;
    auto result = reader.read("whatever.fake", nullptr, limits);

    BOOST_REQUIRE(result.project.has_value());
    BOOST_CHECK_EQUAL(reader.seen.entries.size(), 3);
}

// Nothing to choose from is a failure, not a cancellation, because nobody chose anything.
BOOST_AUTO_TEST_CASE(an_empty_file_fails_rather_than_cancels) {
    FakeReader reader;
    reader.entryCount = 0;
    auto result = reader.read("whatever.fake", nullptr);

    BOOST_CHECK(!result.project.has_value());
    BOOST_CHECK(!result.cancelled);
    BOOST_CHECK(hasError(result.diagnostics));
}

// Closing the chooser is not an error and must not be reported as one, or the editor puts a
// message box in front of a user who just said no.
BOOST_AUTO_TEST_CASE(cancelling_is_not_an_error) {
    FakeReader reader;
    CancellingSelector selector;
    auto result = reader.read("whatever.fake", &selector);

    BOOST_CHECK(!result.project.has_value());
    BOOST_CHECK(result.cancelled);
    BOOST_CHECK(!hasError(result.diagnostics));
}

// A driver that fails in inspect() and forgets to say why would otherwise be indistinguishable
// from a cancellation.
BOOST_AUTO_TEST_CASE(a_failed_inspect_reports_an_error) {
    FakeReader reader;
    reader.readable = false;
    auto result = reader.read("whatever.fake", nullptr);

    BOOST_CHECK(!result.project.has_value());
    BOOST_CHECK(!result.cancelled);
    BOOST_CHECK(hasError(result.diagnostics));
}

BOOST_AUTO_TEST_CASE(the_registry_finds_a_driver_by_suffix) {
    InterchangeRegistry registry;
    BOOST_REQUIRE(registry.addReader(std::make_unique<FakeReader>()));

    BOOST_CHECK(registry.readerForSuffix("fake") != nullptr);
    BOOST_CHECK(registry.readerForSuffix(".fake") != nullptr); // with the dot
    BOOST_CHECK(registry.readerForSuffix("FAKE") != nullptr);  // and without case
    BOOST_CHECK(registry.readerForSuffix("fk") != nullptr);
    BOOST_CHECK(registry.readerForSuffix("mid") == nullptr);
    BOOST_CHECK(registry.readerForId("fake") != nullptr);
}

// A plugin claiming an id the application already registered must not replace it.
BOOST_AUTO_TEST_CASE(the_registry_refuses_a_duplicate_id) {
    InterchangeRegistry registry;
    BOOST_REQUIRE(registry.addReader(std::make_unique<FakeReader>()));
    BOOST_CHECK(!registry.addReader(std::make_unique<FakeReader>()));
    BOOST_CHECK_EQUAL(registry.readers().size(), 1);
}

BOOST_AUTO_TEST_SUITE_END()
