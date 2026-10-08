#include <memory>

#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <hellokit/Interchange/BuiltinInterchangeDrivers.h>
#include <hellokit/Interchange/InterchangeDrivers.h>

#include <Interchange/InterchangeService.h>
#include <Interchange/InterchangeStepPage.h>

using namespace hello;
using namespace hello::daw;

namespace {

    // A page that records its tag in its object name
    class TaggedPage : public InterchangeStepPage {
    public:
        explicit TaggedPage(const QString &tag) {
            setObjectName(tag);
        }

        void reset(const kit::InterchangeReader &, const kit::InterchangeSource &) override {
        }

        bool apply(kit::ImportRequest &) const override {
            return true;
        }
    };

    InterchangeStepRegistry::AddFactory registration(InterchangeService &service, const char *id,
                                                     const char *tag) {
        const auto name = QString::fromLatin1(tag);
        return InterchangeStepRegistry::AddFactory(
            service.stepPages(), id, {}, [name] { return std::make_unique<TaggedPage>(name); });
    }

}

class test_InterchangeService : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The first service that exists is the instance until it is destroyed. A service created
    // while another exists does not replace it.
    void the_first_service_is_the_instance() {
        QVERIFY(!InterchangeService::instance());
        {
            const InterchangeService first;
            QCOMPARE(InterchangeService::instance(), &first);
            {
                const InterchangeService second;
                QCOMPARE(InterchangeService::instance(), &first);
            }
            QCOMPARE(InterchangeService::instance(), &first);
        }
        QVERIFY(!InterchangeService::instance());
    }

    // The drivers registered in the registries of a service are its drivers.
    void the_drivers_are_registered_in_the_service() {
        const InterchangeService service;
        QVERIFY(service.drivers().readers().isEmpty());
        {
            const kit::BuiltinInterchangeDrivers builtins(service.drivers().readerRegistry(),
                                                          service.drivers().writerRegistry());
            QVERIFY(service.drivers().readerForId(QStringLiteral("midi")));
            QVERIFY(service.drivers().writerForId(QStringLiteral("midi")));
        }
        QVERIFY(service.drivers().readers().isEmpty());
    }

    // A new page is created for a registered ID each time, and none for another ID. The page is
    // removed with its registration.
    void a_step_page_is_created_for_its_id() {
        InterchangeService service;
        QVERIFY(!service.stepPages().instantiate("x"));

        auto x = registration(service, "x", "first");
        QVERIFY(x.entry());
        QVERIFY(!registration(service, "x", "second").entry());
        QVERIFY(!service.stepPages().instantiate("z"));
        const auto page = service.stepPages().instantiate("x");
        QVERIFY(page);
        QCOMPARE(page->objectName(), QStringLiteral("first"));
        QVERIFY(page->isComplete());
        QVERIFY(service.stepPages().instantiate("x") != page);

        x = {};
        QVERIFY(!service.stepPages().instantiate("x"));
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_InterchangeService test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_InterchangeService.moc"
