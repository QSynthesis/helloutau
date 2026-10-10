#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Theme/ThemeIcon.h>
#include <helloutau/Widgets/ActionIconToggle.h>

using namespace hello::daw;

namespace {

    // An icon of the theme whose unchecked state draws \a up and checked state \a checked. The
    // files are never drawn here.
    QIcon iconOf(const QString &up, const QString &checked) {
        ThemeIcon icon;
        icon.files.setValue(ThemeButtonState::Up, up);
        icon.files.setValue(ThemeButtonState::CheckedUp, checked);
        return icon.icon();
    }

    // Returns the file that \a action draws in the unchecked state.
    QString shownFile(const QAction &action) {
        const auto icon = ThemeIcon::of(action.icon());
        return icon ? icon->files.value(ThemeButtonState::Up) : QString();
    }

}

class test_ActionIconToggle : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The action stays uncheckable, and its icon shows the checked files while toggled.
    void toggling_shows_the_checked_files() {
        QAction action;
        action.setIcon(iconOf(QStringLiteral("run.svg"), QStringLiteral("pause.svg")));
        const auto key = action.icon().cacheKey();
        const auto toggle = new ActionIconToggle(&action);
        QCOMPARE(action.icon().cacheKey(), key);
        QVERIFY(!toggle->isToggled());

        toggle->setToggled(true);
        QVERIFY(toggle->isToggled());
        QVERIFY(!action.isCheckable());
        QCOMPARE(shownFile(action), QStringLiteral("pause.svg"));

        toggle->setToggled(false);
        QCOMPARE(action.icon().cacheKey(), key);
    }

    // An icon assigned by others replaces the untoggled icon, and its checked files are shown
    // while toggled.
    void an_icon_assigned_meanwhile_becomes_the_untoggled_icon() {
        QAction action;
        action.setIcon(iconOf(QStringLiteral("run.svg"), QStringLiteral("pause.svg")));
        const auto toggle = new ActionIconToggle(&action);
        toggle->setToggled(true);

        const auto replacement = iconOf(QStringLiteral("go.svg"), QStringLiteral("hold.svg"));
        action.setIcon(replacement);
        QCOMPARE(shownFile(action), QStringLiteral("hold.svg"));

        toggle->setToggled(false);
        QCOMPARE(action.icon().cacheKey(), replacement.cacheKey());
    }

    // Other changes of the action, which QAction::changed() reports as well, keep the icon.
    void other_changes_keep_the_icon() {
        QAction action;
        action.setIcon(iconOf(QStringLiteral("run.svg"), QStringLiteral("pause.svg")));
        const auto toggle = new ActionIconToggle(&action);
        toggle->setToggled(true);
        const auto key = action.icon().cacheKey();

        action.setText(QStringLiteral("Play"));
        action.setEnabled(false);
        QCOMPARE(action.icon().cacheKey(), key);
        QCOMPARE(shownFile(action), QStringLiteral("pause.svg"));
    }
};

QTEST_MAIN(test_ActionIconToggle)

#include "test_ActionIconToggle.moc"
