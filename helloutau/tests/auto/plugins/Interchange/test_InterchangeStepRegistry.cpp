#include <memory>

#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <Interchange/InterchangeStepPage.h>
#include <Interchange/InterchangeStepRegistration.h>
#include <Interchange/InterchangeStepRegistry.h>

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

    std::unique_ptr<InterchangeStepRegistration> registration(const char *id, const char *tag) {
        const auto name = QString::fromLatin1(tag);
        return std::make_unique<InterchangeStepRegistration>(
            QString::fromLatin1(id), [name] { return new TaggedPage(name); });
    }

}

class test_InterchangeStepRegistry : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A page is created for a registered ID only, parented to the given widget.
    void a_page_is_created_for_its_id() {
        QVERIFY(!InterchangeStepRegistry::contains(QStringLiteral("x")));
        QVERIFY(!InterchangeStepRegistry::create(QStringLiteral("x")));

        const auto x = registration("x", "first");
        QVERIFY(InterchangeStepRegistry::contains(QStringLiteral("x")));
        QVERIFY(!InterchangeStepRegistry::contains(QStringLiteral("z")));
        QVERIFY(!InterchangeStepRegistry::create(QStringLiteral("z")));
        QWidget parent;
        const auto page = InterchangeStepRegistry::create(QStringLiteral("x"), &parent);
        QVERIFY(page);
        QCOMPARE(page->objectName(), QStringLiteral("first"));
        QCOMPARE(page->parentWidget(), &parent);
        QVERIFY(page->isComplete());
    }

    // Of two registrations of an ID, the first is used until it is destroyed.
    void the_first_registration_of_an_id_is_used() {
        auto first = registration("x", "first");
        const auto second = registration("x", "second");
        std::unique_ptr<InterchangeStepPage> page(InterchangeStepRegistry::create("x"));
        QCOMPARE(page->objectName(), QStringLiteral("first"));

        first.reset();
        page.reset(InterchangeStepRegistry::create("x"));
        QCOMPARE(page->objectName(), QStringLiteral("second"));
    }

    // A destroyed registration is removed from the registry.
    void a_destroyed_registration_is_removed() {
        registration("y", "gone");
        QVERIFY(!InterchangeStepRegistry::contains(QStringLiteral("y")));
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_InterchangeStepRegistry test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_InterchangeStepRegistry.moc"
