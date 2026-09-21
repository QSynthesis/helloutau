#include <memory>

#include <QtTest/QTest>

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
        QVERIFY(registry.readerForSuffix("FAKE") != nullptr);  // and without case
        QVERIFY(registry.readerForSuffix("fk") != nullptr);
        QVERIFY(registry.readerForSuffix("mid") == nullptr);
        QVERIFY(registry.readerForId("fake") != nullptr);
    }

    // A plugin claiming an id the application already registered must not replace it.
    void a_duplicate_id_is_refused() {
        InterchangeRegistry registry;
        QVERIFY(registry.addReader(fake()));
        QVERIFY(!registry.addReader(fake()));
        QCOMPARE(registry.readers().size(), 1);
    }

    // Two drivers may claim one suffix, and the one registered first keeps it, so that a plugin
    // cannot take a format away from the application by claiming it too.
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
