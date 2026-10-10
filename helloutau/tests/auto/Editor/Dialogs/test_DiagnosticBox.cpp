#include <QtCore/QTimer>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <helloutau/Editor/Dialogs/DiagnosticBox.h>

using namespace hello;
using namespace hello::daw;

namespace {

    kit::Diagnostic diagnosticOf(kit::DiagnosticSeverity severity, const char *message,
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
        using S = kit::DiagnosticSeverity;
        const auto iconOf = [](const kit::DiagnosticList &diagnostics) {
            return DiagnosticBox(QStringLiteral("Title"), diagnostics).icon();
        };
        QCOMPARE(iconOf({diagnosticOf(S::Note, "n")}), QMessageBox::Information);
        QCOMPARE(iconOf({diagnosticOf(S::Note, "n"), diagnosticOf(S::Warning, "w")}),
                 QMessageBox::Warning);
        QCOMPARE(iconOf({diagnosticOf(S::Error, "e"), diagnosticOf(S::Warning, "w")}),
                 QMessageBox::Critical);
    }

    // One diagnostic per line, a diagnostic of a note prefixed with the index of the note.
    void each_diagnostic_is_a_line() {
        const DiagnosticBox box(QStringLiteral("Title"),
                                {diagnosticOf(kit::DiagnosticSeverity::Warning, "first", 3),
                                 diagnosticOf(kit::DiagnosticSeverity::Warning, "second")});
        QCOMPARE(box.text(), QStringLiteral("Note 3: first\nsecond"));
        QCOMPARE(box.windowTitle(), QStringLiteral("Title"));
    }

    void an_empty_list_shows_nothing() {
        QVERIFY(!reportShows({}));
        QVERIFY(reportShows({diagnosticOf(kit::DiagnosticSeverity::Note, "n")}));
    }
};

QTEST_MAIN(test_DiagnosticBox)

#include "test_DiagnosticBox.moc"
