#include <memory>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Interchange/BuiltinInterchangeDrivers.h>
#include <hellokit/Interchange/InterchangeRegistration.h>
#include <hellokit/Interchange/InterchangeRegistry.h>

using namespace hello::kit;

namespace {

    // Implements only the properties used by the registry: the ID and the suffixes.
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

    class NamedWriter : public InterchangeWriter {
    public:
        explicit NamedWriter(QString id, QStringList suffixes)
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

    protected:
        bool convert(const Project &, const std::filesystem::path &, const ExportRequest &,
                     DiagnosticList &) override {
            return false;
        }

    private:
        QString m_id;
        QStringList m_suffixes;
    };

    std::unique_ptr<InterchangeRegistration> reader(const char *id,
                                                    QStringList suffixes = {QStringLiteral("x")}) {
        return std::make_unique<InterchangeRegistration>(
            std::make_unique<NamedReader>(QString::fromLatin1(id), std::move(suffixes)));
    }

    std::unique_ptr<InterchangeRegistration> writer(const char *id,
                                                    QStringList suffixes = {QStringLiteral("x")}) {
        return std::make_unique<InterchangeRegistration>(
            std::make_unique<NamedWriter>(QString::fromLatin1(id), std::move(suffixes)));
    }

    template <class T>
    QStringList idsOf(const QList<T *> &drivers) {
        QStringList ids;
        for (const auto driver : drivers) {
            ids.push_back(driver->id());
        }
        return ids;
    }

}

class test_InterchangeRegistry : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_driver_is_found_by_suffix() {
        InterchangeRegistry registry;
        const auto fake = reader("fake", {QStringLiteral("fake"), QStringLiteral("FK")});

        QVERIFY(registry.readerForSuffix("fake") != nullptr);
        QVERIFY(registry.readerForSuffix(".fake") != nullptr); // with the dot
        QVERIFY(registry.readerForSuffix("FAKE") != nullptr);  // and case-insensitively
        QVERIFY(registry.readerForSuffix("fk") != nullptr);
        QVERIFY(registry.readerForSuffix("mid") == nullptr);
        QCOMPARE(registry.readerForId("fake"), fake->reader());
        QVERIFY(registry.writerForSuffix("fake") == nullptr);
        QVERIFY(registry.writerForId("fake") == nullptr);
    }

    // A second registration of an ID does not replace the first. After the first registration is
    // destroyed, the second driver is used.
    void of_two_drivers_of_one_id_the_first_is_used() {
        InterchangeRegistry registry;
        auto first = reader("fake");
        const auto second = reader("fake");
        QCOMPARE(registry.readers().size(), 1);
        QCOMPARE(registry.readerForId("fake"), first->reader());

        first.reset();
        QCOMPARE(registry.readers().size(), 1);
        QCOMPARE(registry.readerForId("fake"), second->reader());
    }

    // Of two drivers with the same suffix, the first registered is used for the suffix.
    void the_first_to_claim_a_suffix_keeps_it() {
        InterchangeRegistry registry;
        const auto first = writer("first", {QStringLiteral("mid")});
        const auto second = writer("second", {QStringLiteral("mid")});

        const auto found = registry.writerForSuffix("mid");
        QVERIFY(found != nullptr);
        QCOMPARE(found->id(), QStringLiteral("first"));
    }

    // A registry lists registrations made before and after its construction, in the order of
    // registration, with readers and writers separate, and emits driversChanged() on each change.
    void registries_follow_the_registrations() {
        const auto before = reader("before");
        InterchangeRegistry registry;
        QSignalSpy changed(&registry, &InterchangeRegistry::driversChanged);
        QCOMPARE(idsOf(registry.readers()), QStringList({"before"}));
        QVERIFY(registry.writers().isEmpty());

        auto builtins = std::make_unique<BuiltinInterchangeDrivers>();
        QCOMPARE(changed.size(), 2);
        QCOMPARE(idsOf(registry.readers()), QStringList({"before", "midi"}));
        QCOMPARE(idsOf(registry.writers()), QStringList({"midi"}));
        QCOMPARE(registry.readerForSuffix("mid")->id(), QStringLiteral("midi"));
        QCOMPARE(registry.writerForSuffix("mid")->id(), QStringLiteral("midi"));

        auto after = writer("after");
        QCOMPARE(changed.size(), 3);
        const InterchangeRegistry later;
        QCOMPARE(idsOf(later.writers()), QStringList({"midi", "after"}));

        builtins.reset();
        after.reset();
        QCOMPARE(changed.size(), 6);
        QCOMPARE(idsOf(registry.readers()), QStringList({"before"}));
        QVERIFY(registry.writers().isEmpty());
        QVERIFY(!registry.readerForSuffix("mid"));
    }
};

QTEST_APPLESS_MAIN(test_InterchangeRegistry)

#include "test_InterchangeRegistry.moc"
