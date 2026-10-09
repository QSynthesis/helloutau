#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>

#include <helloutau/Editor/Dialogs/VoiceAliasRuleDialog.h>

using namespace hello;
using namespace hello::daw;

namespace {

    // Two directories: a.wav with x and x_2 in the first, b.wav with an empty alias in the second
    QList<VoiceAliasRuleDialog::Group> groups() {
        VoiceAliasRuleDialog::Group first;
        first.entries = {
            {"a.wav", "x"  },
            {"a.wav", "x_2"}
        };
        first.selected = {0};
        VoiceAliasRuleDialog::Group second;
        second.entries = {
            {"b.wav", ""}
        };
        second.selected = {0};
        return {first, second};
    }

    QStringList column(const QTreeWidget *tree, int column) {
        QStringList texts;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            texts.push_back(tree->topLevelItem(i)->text(column));
        }
        return texts;
    }

}

class test_VoiceAliasRuleDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The preview lists each selected entry of each group, and OK is enabled only for a rule
    // with changes and without problems.
    void the_preview_follows_the_rule() {
        VoiceAliasRuleDialog dialog(VoiceAliasRuleDialog::Rename, groups());
        QVERIFY(!dialog.okButton()->isEnabled());
        QCOMPARE(column(dialog.preview(), 0), QStringList({"x", "b"}));

        dialog.textEdit()->setText(QStringLiteral("_2"));
        QCOMPARE(column(dialog.preview(), 1), QStringList({"x_2", "b_2"}));
        QVERIFY(!column(dialog.preview(), 2).first().isEmpty());
        QVERIFY(!dialog.okButton()->isEnabled());

        dialog.kindBox()->setCurrentIndex(
            dialog.kindBox()->findData(kit::VoiceAliasRule::AddPrefix));
        QCOMPARE(column(dialog.preview(), 1), QStringList({"_2x", "_2b"}));
        QCOMPARE(column(dialog.preview(), 2), QStringList({"", ""}));
        QVERIFY(dialog.okButton()->isEnabled());
        QCOMPARE(dialog.rule().kind, kit::VoiceAliasRule::AddPrefix);
        QCOMPARE(dialog.rule().text, QStringLiteral("_2"));
    }

    // The replacement field is shown for Replace only, and a rule without an effect leaves OK
    // disabled.
    void the_replacement_is_shown_for_replace() {
        VoiceAliasRuleDialog dialog(VoiceAliasRuleDialog::Rename, groups());
        dialog.show();
        QVERIFY(!dialog.replacementEdit()->isVisible());
        dialog.kindBox()->setCurrentIndex(dialog.kindBox()->findData(kit::VoiceAliasRule::Replace));
        QVERIFY(dialog.replacementEdit()->isVisible());
        dialog.textEdit()->setText(QStringLiteral("q"));
        QVERIFY(!dialog.okButton()->isEnabled());
        dialog.textEdit()->setText(QStringLiteral("x"));
        dialog.replacementEdit()->setText(QStringLiteral("y"));
        QCOMPARE(column(dialog.preview(), 1), QStringList({"y", "b"}));
        QVERIFY(dialog.okButton()->isEnabled());
    }

    // For a copy, an unchanged name conflicts with the original.
    void a_copy_needs_a_new_name() {
        VoiceAliasRuleDialog dialog(VoiceAliasRuleDialog::Duplicate, groups());
        dialog.kindBox()->setCurrentIndex(
            dialog.kindBox()->findData(kit::VoiceAliasRule::RemoveSuffix));
        dialog.textEdit()->setText(QStringLiteral("_9"));
        QVERIFY(!dialog.okButton()->isEnabled());
        QCOMPARE(dialog.changes().size(), 2);
        QVERIFY(!dialog.changes()[0][0].problem.isEmpty());
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_VoiceAliasRuleDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_VoiceAliasRuleDialog.moc"
