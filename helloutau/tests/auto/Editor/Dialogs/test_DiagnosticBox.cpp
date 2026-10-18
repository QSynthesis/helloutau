#include <QtCore/QTimer>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/Dialogs/DiagnosticBox.h>

using namespace hello;
using namespace hello::daw;

namespace {

    kit::Diagnostic diagnosticOf(kit::Diagnostic::Severity severity, const char *message,
                                 std::optional<int> note = std::nullopt) {
        return {severity, QString::fromLatin1(message), note};
    }

    // Returns whether report() shows a box for \a diagnostics, which is closed at once.
    bool reportShows(const kit::DiagnosticList &diagnostics) {
        bool shown = false;
        QTimer::singleShot(0, [&shown] {
            if (const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                shown = true;
                box->done(QMessageBox::Ok);
            }
        });
        DiagnosticBox::report(nullptr, QStringLiteral("Title"), diagnostics);
        // The single shot fires after report() if no box ran an event loop.
        QCoreApplication::processEvents();
        return shown;
    }

}

class test_DiagnosticBox : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The icon is that of the most severe diagnostic.
    void the_icon_follows_the_most_severe_diagnostic() {
        using S = kit::Diagnostic::Severity;
        const auto iconOf = [](const kit::DiagnosticList &diagnostics) {
            return DiagnosticBox(QStringLiteral("Title"), diagnostics).icon();
        };
        QCOMPARE(iconOf({diagnosticOf(S::Note, "n")}), QMessageBox::Information);
        QCOMPARE(iconOf({diagnosticOf(S::Note, "n"), diagnosticOf(S::Warning, "w")}),
                 QMessageBox::Warning);
        QCOMPARE(iconOf({diagnosticOf(S::Error, "e"), diagnosticOf(S::Warning, "w")}),
                 QMessageBox::Critical);
    }

    // One diagnostic per line, a diagnostic of a note prefixed with the number of the note,
    // counted from 1.
    void each_diagnostic_is_a_line() {
        const DiagnosticBox box(QStringLiteral("Title"),
                                {diagnosticOf(kit::Diagnostic::Warning, "first", 3),
                                 diagnosticOf(kit::Diagnostic::Warning, "second")});
        QCOMPARE(box.text(), QStringLiteral("Note 4: first\nsecond"));
        // QMessageBox::setWindowTitle() ignores the title on macOS, whose message boxes have none.
#ifndef Q_OS_MACOS
        QCOMPARE(box.windowTitle(), QStringLiteral("Title"));
#endif
    }

    void an_empty_list_shows_nothing() {
        QVERIFY(!reportShows({}));
        QVERIFY(reportShows({diagnosticOf(kit::Diagnostic::Note, "n")}));
    }

    // The summary and the probable causes come first, with their remedies, and the diagnostics
    // move to the details. The texts are escaped, because the causes name paths.
    void the_probable_causes_precede_the_diagnostics() {
        const kit::SynthDiagnosis diagnosis{
            QStringLiteral("cause <a>"),
            QStringLiteral("explanation"),
            {QStringLiteral("first remedy"), QStringLiteral("second remedy")},
            {0}
        };
        const DiagnosticBox box(QStringLiteral("Title"), QStringLiteral("summary <b>"),
                                {diagnosticOf(kit::Diagnostic::Note, "detail", 1)},
                                {diagnosis});
        QCOMPARE(box.textFormat(), Qt::RichText);
        QVERIFY(box.text().startsWith(QStringLiteral("<p>summary &lt;b&gt;</p>")));
        // The causes are the items of a bulleted list, or of a numbered list on request.
        QVERIFY(box.text().contains(QStringLiteral("<ul><li><b>cause &lt;a&gt;</b>")));
        const DiagnosticBox ordered(QStringLiteral("Title"), {}, {}, {diagnosis},
                                    DiagnosticBox::OrderedList);
        QVERIFY(ordered.text().startsWith(QStringLiteral("<p><b>")));
        QVERIFY(ordered.text().contains(QStringLiteral("<ol><li><b>cause &lt;a&gt;</b>")));
        QVERIFY(ordered.text().endsWith(QStringLiteral("</ol>")));
        QVERIFY(box.text().contains(QStringLiteral("explanation")));
        QVERIFY(box.text().indexOf(QStringLiteral("first remedy")) <
                box.text().indexOf(QStringLiteral("second remedy")));
        QCOMPARE(box.detailedText(), QStringLiteral("Note 2: detail"));
        QCOMPARE(box.icon(), QMessageBox::Warning);
    }

    void without_causes_the_box_is_unchanged() {
        const DiagnosticBox box(QStringLiteral("Title"), QStringLiteral("summary"),
                                {diagnosticOf(kit::Diagnostic::Note, "n")}, {});
        QCOMPARE(box.text(), QStringLiteral("n"));
        QVERIFY(box.detailedText().isEmpty());
        QCOMPARE(box.icon(), QMessageBox::Information);
    }
};

QTEST_MAIN(test_DiagnosticBox)

#include "test_DiagnosticBox.moc"
