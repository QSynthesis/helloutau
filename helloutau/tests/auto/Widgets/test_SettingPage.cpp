#include <QtCore/QPointer>
#include <QtTest/QTest>

#include <helloutau/Widgets/SettingPage.h>

using namespace hello::daw;

namespace {

    SettingPage *pageOf(const char *id) {
        return new SettingPage(QString::fromLatin1(id));
    }

}

class test_SettingPage : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // A page is removed from the top level or from any level below, and deleted with its
    // children. A page that is not in the catalog is left alone.
    void a_page_is_removed_from_any_level() {
        SettingCatalog catalog;
        const auto top = pageOf("top");
        const auto child = pageOf("top.child");
        const auto grandchild = pageOf("top.child.grandchild");
        const auto other = pageOf("other");
        child->addPage(grandchild);
        top->addPage(child);
        catalog.addPage(top);
        catalog.addPage(other);

        QPointer<SettingPage> watchedGrandchild = grandchild;
        catalog.removePage(grandchild);
        QVERIFY(!watchedGrandchild);
        QCOMPARE(catalog.page(QStringLiteral("top.child.grandchild")), nullptr);
        QVERIFY(child->pages().isEmpty());

        QPointer<SettingPage> watchedChild = child;
        catalog.removePage(child);
        QVERIFY(!watchedChild);
        QCOMPARE(catalog.allPages(), (QList<SettingPage *>{top, other}));

        QPointer<SettingPage> watchedTop = top;
        catalog.removePage(top);
        QVERIFY(!watchedTop);
        QCOMPARE(catalog.pages(), QList<SettingPage *>{other});

        SettingPage outside(QStringLiteral("outside"));
        catalog.removePage(&outside);
        QCOMPARE(catalog.allPages(), QList<SettingPage *>{other});

        // A child of a page outside the catalog stays with its parent.
        const auto outsideChild = pageOf("outside.child");
        outside.addPage(outsideChild);
        catalog.removePage(outsideChild);
        QCOMPARE(outside.pages(), QList<SettingPage *>{outsideChild});
    }

    // Removing a page also deletes its children.
    void removing_a_page_deletes_its_children() {
        SettingCatalog catalog;
        const auto top = pageOf("top");
        const auto child = pageOf("top.child");
        top->addPage(child);
        catalog.addPage(top);
        QPointer<SettingPage> watched = child;
        catalog.removePage(top);
        QVERIFY(!watched);
        QVERIFY(catalog.allPages().isEmpty());
    }
};

QTEST_APPLESS_MAIN(test_SettingPage)

#include "test_SettingPage.moc"
