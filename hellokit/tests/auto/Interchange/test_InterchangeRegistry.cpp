#include <memory>

#include <QtTest/QTest>

#include <hellokit/Interchange/InterchangeRegistry.h>

using namespace hello::kit;

namespace {

    // Only the properties the registry uses: the ID and the registered suffixes.
    class NamedReader : public InterchangeReader {
    public:
        explicit NamedReader(QString id, QStringList suffixes)
            : m_id(std::move(id)), m_suffixes(std::move(suffixes)) {
        }

        QString id() const override {
            return m_id;
        }
        QString name() const override {
            return m_id;
        }
        QStringList suffixes() const override {
            return m_suffixes;
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
        QString m_id;
        QStringList m_suffixes;
    };

    std::unique_ptr<InterchangeReader> fake() {
        return std::make_unique<NamedReader>(
            QStringLiteral("fake"), QStringList{QStringLiteral("fake"), QStringLiteral("FK")});
    }

}

class test_InterchangeRegistry : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_driver_is_found_by_suffix() {
        InterchangeRegistry registry;
        QVERIFY(registry.addReader(fake()));

        QVERIFY(registry.readerForSuffix("fake") != nullptr);
        QVERIFY(registry.readerForSuffix(".fake") != nullptr); // with the dot
        QVERIFY(registry.readerForSuffix("FAKE") != nullptr);  // and case-insensitively
        QVERIFY(registry.readerForSuffix("fk") != nullptr);
        QVERIFY(registry.readerForSuffix("mid") == nullptr);
        QVERIFY(registry.readerForId("fake") != nullptr);
    }

    // A plugin registering an ID that the application already registered must not replace it.
    void a_duplicate_id_is_refused() {
        InterchangeRegistry registry;
        QVERIFY(registry.addReader(fake()));
        QVERIFY(!registry.addReader(fake()));
        QCOMPARE(registry.readers().size(), 1);
    }

    // Two drivers may register the same suffix, and the first registration takes precedence,
    // so that a plugin cannot take over a built-in format.
    void the_first_to_claim_a_suffix_keeps_it() {
        InterchangeRegistry registry;
        QVERIFY(registry.addReader(std::make_unique<NamedReader>(
            QStringLiteral("first"), QStringList{QStringLiteral("mid")})));
        QVERIFY(registry.addReader(std::make_unique<NamedReader>(
            QStringLiteral("second"), QStringList{QStringLiteral("mid")})));

        const auto *found = registry.readerForSuffix("mid");
        QVERIFY(found != nullptr);
        QCOMPARE(found->id(), QStringLiteral("first"));
    }
};

QTEST_APPLESS_MAIN(test_InterchangeRegistry)

#include "test_InterchangeRegistry.moc"
