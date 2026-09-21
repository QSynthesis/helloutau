#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE test_InterchangeReader

#include <boost/test/unit_test.hpp>

#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeSelector.h>

using namespace hello::kit;

namespace {

    // A driver with no format behind it, so that the flow read() runs can be watched without a
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
            return {QStringLiteral("fake")};
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
            InterchangeEntry entry;
            entry.index = 0;
            entry.noteCount = 1;
            source.entries.push_back(entry);
            return source;
        }

        bool readable = true;
        mutable bool converted = false;

    protected:
        std::optional<Project> convert(const std::filesystem::path &, const InterchangeSource &,
                                       const ImportRequest &, DiagnosticList &) override {
            converted = true;
            Project project;
            project.tracks.push_back({});
            return project;
        }
    };

    class CancellingSelector : public InterchangeSelector {
    public:
        std::optional<ImportRequest> selectImport(const InterchangeReader &,
                                                  const InterchangeSource &, const ImportLimits &,
                                                  DiagnosticList &) override {
            return std::nullopt;
        }
        std::optional<ExportRequest> selectExport(const InterchangeWriter &, const Project &,
                                                  DiagnosticList &) override {
            return std::nullopt;
        }
    };

}

BOOST_AUTO_TEST_SUITE(test_InterchangeReader)

// Handed no selector, a driver still has to finish. The command line and the tests have nobody
// to ask, and a test that reached a dialog would hang rather than fail.
BOOST_AUTO_TEST_CASE(no_selector_still_finishes) {
    FakeReader reader;
    auto result = reader.read("whatever.fake", nullptr);

    BOOST_REQUIRE(result.project.has_value());
    BOOST_CHECK(!result.cancelled);
    BOOST_CHECK(reader.converted);
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
    BOOST_CHECK(!reader.converted);
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

BOOST_AUTO_TEST_SUITE_END()
