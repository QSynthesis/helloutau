#include <QtTest/QTest>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTreeWidget>

#include <hellokit/VoiceBank/FrequencyFormat.h>

#include <helloutau/Editor/Dialogs/ConvertFrequencyMapsDialog.h>
#include <helloutau/Widgets/FileCheckList.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    using File = ConvertFrequencyMapsDialog::File;

    // A format with an identifier, which writes maps if writable is set
    class Format : public kit::FrequencyFormat {
    public:
        Format(QString id, bool writable) : m_id(std::move(id)), m_writable(writable) {
        }

        QString id() const override {
            return m_id;
        }
        QString name() const override {
            return m_id;
        }
        QStringList resamplerPatterns() const override {
            return {};
        }
        QList<bool> mapsExist(const fs::path &, const QList<fs::path> &wavs) const override {
            return QList<bool>(wavs.size(), false);
        }
        std::optional<kit::FrequencyMap> read(const fs::path &, int,
                                              kit::DiagnosticList &) const override {
            return std::nullopt;
        }
        bool canWrite() const override {
            return m_writable;
        }

    private:
        QString m_id;
        bool m_writable;
    };

    QStringList itemsOf(const QComboBox *box) {
        QStringList items;
        for (int i = 0; i < box->count(); ++i) {
            items.push_back(box->itemText(i));
        }
        return items;
    }

    // Returns a.wav with a map of the first format, b.wav with a map of the second, and
    // sub/c.wav without a map, selected if cSelected.
    QList<File> filesOf(bool cSelected = false) {
        return {
            {fs::path("C:/bank/a.wav"),     {true, false},  false    },
            {fs::path("C:/bank/b.wav"),     {false, true},  false    },
            {fs::path("C:/bank/sub/c.wav"), {false, false}, cSelected},
        };
    }

}

class test_ConvertFrequencyMapsDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Every format is a source format, and only the formats that write maps are target formats.
    // The initial target format is the first writable format other than the source format.
    void the_target_formats_write_maps() {
        Format a(QStringLiteral("a"), true);
        Format b(QStringLiteral("b"), false);
        Format c(QStringLiteral("c"), true);
        const ConvertFrequencyMapsDialog dialog({}, "C:/bank", {&a, &b, &c}, &a);
        QCOMPARE(itemsOf(dialog.sourceBox()), (QStringList{"a", "b", "c"}));
        QCOMPARE(itemsOf(dialog.targetBox()), (QStringList{"a", "c"}));
        QCOMPARE(dialog.sourceFormat(), &a);
        QCOMPARE(dialog.targetFormat(), &c);
    }

    // The conversion requires a checked file with a map of the source format, and two different
    // formats.
    void the_formats_must_differ() {
        Format a(QStringLiteral("a"), true);
        Format b(QStringLiteral("b"), true);
        const ConvertFrequencyMapsDialog dialog(filesOf(), "C:/bank", {&a, &b}, &b);
        const auto ok = dialog.buttonBox()->button(QDialogButtonBox::Ok);
        QCOMPARE(dialog.targetFormat(), &a);
        QVERIFY(ok->isEnabled());
        dialog.targetBox()->setCurrentIndex(1);
        QCOMPARE(dialog.targetFormat(), &b);
        QVERIFY(!ok->isEnabled());
        dialog.targetBox()->setCurrentIndex(0);
        dialog.fileList()->checkWhere([](int) { return false; });
        QVERIFY(!ok->isEnabled());

        // A checked file without a map of the source format is skipped, and converts nothing.
        dialog.fileList()->checkWhere([](int row) { return row == 2; });
        QVERIFY(!ok->isEnabled());
        dialog.fileList()->setChecked(1, true);
        QVERIFY(ok->isEnabled());
    }

    // Without a writable format there is no target format.
    void no_target_without_a_writable_format() {
        Format a(QStringLiteral("a"), false);
        const ConvertFrequencyMapsDialog dialog(filesOf(), "C:/bank", {&a}, &a);
        QCOMPARE(dialog.targetFormat(), nullptr);
        QVERIFY(!dialog.buttonBox()->button(QDialogButtonBox::Ok)->isEnabled());
    }

    // Without a selection, the files with a map of the source format are checked, again when
    // the source format changes. A button checks them after the user has checked others.
    void the_files_with_source_maps_are_checked() {
        Format a(QStringLiteral("a"), true);
        Format b(QStringLiteral("b"), true);
        ConvertFrequencyMapsDialog dialog(filesOf(), "C:/bank", {&a, &b}, &a);
        QCOMPARE(dialog.checkedFiles(), QList<fs::path>{fs::path("C:/bank/a.wav")});
        dialog.sourceBox()->setCurrentIndex(1);
        QCOMPARE(dialog.checkedFiles(), QList<fs::path>{fs::path("C:/bank/b.wav")});

        dialog.fileList()->checkWhere([](int) { return true; });
        dialog.checkMapsButton()->click();
        QCOMPARE(dialog.checkedFiles(), QList<fs::path>{fs::path("C:/bank/b.wav")});
    }

    // With a selection, the selected files are checked and stay checked when the source format
    // changes.
    void the_selected_files_are_checked() {
        Format a(QStringLiteral("a"), true);
        Format b(QStringLiteral("b"), true);
        ConvertFrequencyMapsDialog dialog(filesOf(true), "C:/bank", {&a, &b}, &a);
        QCOMPARE(dialog.checkedFiles(), QList<fs::path>{fs::path("C:/bank/sub/c.wav")});
        dialog.sourceBox()->setCurrentIndex(1);
        QCOMPARE(dialog.checkedFiles(), QList<fs::path>{fs::path("C:/bank/sub/c.wav")});
    }

    // Each format has a centered column named by the format, with a mark for a file that has a
    // map of the format and nothing for a file that has none.
    void each_format_has_a_column() {
        Format a(QStringLiteral("a"), true);
        Format b(QStringLiteral("b"), true);
        const ConvertFrequencyMapsDialog dialog(filesOf(), "C:/bank", {&a, &b}, &a);
        const auto tree = dialog.fileList()->tree();
        QCOMPARE(tree->columnCount(), 3);
        QCOMPARE(tree->headerItem()->text(1), QStringLiteral("a"));
        QCOMPARE(tree->headerItem()->text(2), QStringLiteral("b"));
        QVERIFY(!tree->topLevelItem(0)->text(1).isEmpty());
        QVERIFY(tree->topLevelItem(0)->text(2).isEmpty());
        QVERIFY(tree->topLevelItem(1)->text(1).isEmpty());
        QVERIFY(!tree->topLevelItem(1)->text(2).isEmpty());
        QCOMPARE(tree->headerItem()->textAlignment(1), int(Qt::AlignCenter));
        QCOMPARE(tree->topLevelItem(0)->textAlignment(2), int(Qt::AlignCenter));
    }

    // The parenthesized part of the name of a format, such as its resampler, is on the second
    // line of the header, and the boxes show the whole name.
    void the_resampler_is_on_a_line_of_its_own() {
        Format a(QStringLiteral("frq (resampler)"), true);
        Format b(QStringLiteral("dio"), true);
        const ConvertFrequencyMapsDialog dialog(filesOf(), "C:/bank", {&a, &b}, &a);
        const auto tree = dialog.fileList()->tree();
        QCOMPARE(tree->headerItem()->text(1), QStringLiteral("frq\n(resampler)"));
        QCOMPARE(tree->headerItem()->text(2), QStringLiteral("dio"));
        QCOMPARE(dialog.sourceBox()->itemText(0), QStringLiteral("frq (resampler)"));
    }

    // The files and the formats are the panes of a splitter, which the user resizes and cannot
    // collapse.
    void the_files_and_the_formats_are_split() {
        Format a(QStringLiteral("a"), true);
        const ConvertFrequencyMapsDialog dialog(filesOf(), "C:/bank", {&a}, &a);
        const auto splitter = dialog.splitter();
        QCOMPARE(splitter->count(), 2);
        QCOMPARE(splitter->widget(0), dialog.fileList());
        QVERIFY(splitter->widget(1)->isAncestorOf(dialog.sourceBox()));
        QVERIFY(!splitter->childrenCollapsible());
    }
};

QTEST_MAIN(test_ConvertFrequencyMapsDialog)

#include "test_ConvertFrequencyMapsDialog.moc"
