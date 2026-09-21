#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE test_InterchangeRegistry

#include <memory>

#include <boost/test/unit_test.hpp>

#include <hellokit/Interchange/InterchangeRegistry.h>

using namespace hello::kit;

namespace {

    // Only what the registry looks at: the name it is filed under and the suffixes it claims.
    class NamedReader : public InterchangeReader {
    public:
        explicit NamedReader(QString id, QStringList suffixes)
            : _id(std::move(id)), _suffixes(std::move(suffixes)) {
        }

        QString id() const override {
            return _id;
        }
        QString name() const override {
            return _id;
        }
        QStringList suffixes() const override {
            return _suffixes;
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

    private:
        QString _id;
        QStringList _suffixes;
    };

    std::unique_ptr<InterchangeReader> fake() {
        return std::make_unique<NamedReader>(QStringLiteral("fake"),
                                             QStringList{QStringLiteral("fake"),
                                                         QStringLiteral("FK")});
    }

}

BOOST_AUTO_TEST_SUITE(test_InterchangeRegistry)

BOOST_AUTO_TEST_CASE(a_driver_is_found_by_suffix) {
    InterchangeRegistry registry;
    BOOST_REQUIRE(registry.addReader(fake()));

    BOOST_CHECK(registry.readerForSuffix("fake") != nullptr);
    BOOST_CHECK(registry.readerForSuffix(".fake") != nullptr); // with the dot
    BOOST_CHECK(registry.readerForSuffix("FAKE") != nullptr);  // and without case
    BOOST_CHECK(registry.readerForSuffix("fk") != nullptr);
    BOOST_CHECK(registry.readerForSuffix("mid") == nullptr);
    BOOST_CHECK(registry.readerForId("fake") != nullptr);
}

// A plugin claiming an id the application already registered must not replace it.
BOOST_AUTO_TEST_CASE(a_duplicate_id_is_refused) {
    InterchangeRegistry registry;
    BOOST_REQUIRE(registry.addReader(fake()));
    BOOST_CHECK(!registry.addReader(fake()));
    BOOST_CHECK_EQUAL(registry.readers().size(), 1);
}

// Two drivers may claim one suffix, and the one registered first keeps it, so that a plugin
// cannot take a format away from the application by claiming it too.
BOOST_AUTO_TEST_CASE(the_first_to_claim_a_suffix_keeps_it) {
    InterchangeRegistry registry;
    BOOST_REQUIRE(registry.addReader(std::make_unique<NamedReader>(
        QStringLiteral("first"), QStringList{QStringLiteral("mid")})));
    BOOST_REQUIRE(registry.addReader(std::make_unique<NamedReader>(
        QStringLiteral("second"), QStringList{QStringLiteral("mid")})));

    const auto *found = registry.readerForSuffix("mid");
    BOOST_REQUIRE(found != nullptr);
    BOOST_CHECK_EQUAL(found->id().toStdString(), "first");
}

BOOST_AUTO_TEST_SUITE_END()
