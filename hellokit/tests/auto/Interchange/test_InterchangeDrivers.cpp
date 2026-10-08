#include <memory>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Interchange/BuiltinInterchangeDrivers.h>
#include <hellokit/Interchange/InterchangeDrivers.h>

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

    InterchangeReaderRegistry::AddFactory reader(InterchangeDrivers &drivers, const char *id,
                                                 QStringList suffixes = {QStringLiteral("x")}) {
        return InterchangeReaderRegistry::AddFactory(
            drivers.readerRegistry(), id, {}, [id, suffixes] {
                return std::make_unique<NamedReader>(QString::fromLatin1(id), suffixes);
            });
    }

    InterchangeWriterRegistry::AddFactory writer(InterchangeDrivers &drivers, const char *id,
                                                 QStringList suffixes = {QStringLiteral("x")}) {
        return InterchangeWriterRegistry::AddFactory(
            drivers.writerRegistry(), id, {}, [id, suffixes] {
                return std::make_unique<NamedWriter>(QString::fromLatin1(id), suffixes);
            });
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

class test_InterchangeDrivers : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_driver_is_found_by_suffix() {
        InterchangeDrivers drivers;
        const auto fake = reader(drivers, "fake", {QStringLiteral("fake"), QStringLiteral("FK")});

        QVERIFY(drivers.readerForSuffix("fake") != nullptr);
        QVERIFY(drivers.readerForSuffix(".fake") != nullptr); // with the dot
        QVERIFY(drivers.readerForSuffix("FAKE") != nullptr);  // and case-insensitively
        QVERIFY(drivers.readerForSuffix("fk") != nullptr);
        QVERIFY(drivers.readerForSuffix("mid") == nullptr);
        QCOMPARE(drivers.readerForId("fake")->id(), QStringLiteral("fake"));
        QVERIFY(drivers.writerForSuffix("fake") == nullptr);
        QVERIFY(drivers.writerForId("fake") == nullptr);
    }

    // A second registration of an ID fails, and a driver whose ID differs from its name is
    // rejected.
    void a_driver_needs_a_unique_name_equal_to_its_id() {
        InterchangeDrivers drivers;
        const auto first = reader(drivers, "fake");
        const auto second = reader(drivers, "fake");
        QVERIFY(first.entry());
        QVERIFY(!second.entry());
        const InterchangeReaderRegistry::AddFactory renamed(
            drivers.readerRegistry(), "renamed", {},
            [] { return std::make_unique<NamedReader>(QStringLiteral("other"), QStringList()); });
        QVERIFY(renamed.entry());
        QCOMPARE(idsOf(drivers.readers()), QStringList({"fake"}));
    }

    // Of two drivers with the same suffix, the first registered is used for the suffix.
    void the_first_to_claim_a_suffix_keeps_it() {
        InterchangeDrivers drivers;
        const auto first = writer(drivers, "first", {QStringLiteral("mid")});
        const auto second = writer(drivers, "second", {QStringLiteral("mid")});

        const auto found = drivers.writerForSuffix("mid");
        QVERIFY(found != nullptr);
        QCOMPARE(found->id(), QStringLiteral("first"));
    }

    // The drivers follow the registries in the order of registration, with readers and writers
    // separate, and driversChanged() is emitted after each change. Each object has registries of
    // its own.
    void the_drivers_follow_the_registries() {
        InterchangeDrivers drivers;
        QSignalSpy changed(&drivers, &InterchangeDrivers::driversChanged);
        QList<QStringList> seen;
        connect(&drivers, &InterchangeDrivers::driversChanged,
                [&] { seen.push_back(idsOf(drivers.readers())); });
        const auto before = reader(drivers, "before");
        QCOMPARE(idsOf(drivers.readers()), QStringList({"before"}));
        QCOMPARE(seen, QList<QStringList>({{"before"}}));
        QVERIFY(drivers.writers().isEmpty());

        auto builtins = std::make_unique<BuiltinInterchangeDrivers>(drivers.readerRegistry(),
                                                                    drivers.writerRegistry());
        QCOMPARE(changed.size(), 3);
        QCOMPARE(idsOf(drivers.readers()), QStringList({"before", "midi"}));
        QCOMPARE(idsOf(drivers.writers()), QStringList({"midi"}));
        QCOMPARE(drivers.readerForSuffix("mid")->id(), QStringLiteral("midi"));
        QCOMPARE(drivers.writerForSuffix("mid")->id(), QStringLiteral("midi"));

        auto after = writer(drivers, "after");
        QCOMPARE(changed.size(), 4);
        QCOMPARE(idsOf(drivers.writers()), QStringList({"midi", "after"}));
        const InterchangeDrivers other;
        QVERIFY(other.readers().isEmpty() && other.writers().isEmpty());

        builtins.reset();
        after = {};
        QCOMPARE(changed.size(), 7);
        QCOMPARE(idsOf(drivers.readers()), QStringList({"before"}));
        QVERIFY(drivers.writers().isEmpty());
        QVERIFY(!drivers.readerForSuffix("mid"));
    }
};

QTEST_APPLESS_MAIN(test_InterchangeDrivers)

#include "test_InterchangeDrivers.moc"
