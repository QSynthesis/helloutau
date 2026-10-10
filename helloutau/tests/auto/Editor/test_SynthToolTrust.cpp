#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtTest/QTest>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/SynthToolTrust.h>

using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    void writeFile(const QString &path, const QByteArray &content) {
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content);
    }

    // Answers the next message box with \a button and records its text in \a text.
    void answerNextBox(QMessageBox::StandardButton button, QString *text, bool *shown) {
        QTimer::singleShot(0, [button, text, shown] {
            if (const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                *shown = true;
                *text = box->text();
                box->button(button)->click();
            }
        });
    }

}

// The synth tool paths of a project are untrusted input: a synth tool runs only if it is one of
// the settings or the user has trusted its file, see the security section of CLAUDE.md.
class test_SynthToolTrust : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<AppSettings> m_settings;
    fs::path m_utau;

    QString pathIn(const char *relative) const {
        return QDir::toNativeSeparators(m_dir->filePath(QString::fromLatin1(relative)));
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        m_settings =
            std::make_unique<AppSettings>(m_dir->filePath(QStringLiteral("settings.json")));
        m_utau = fs::path(m_dir->filePath(QStringLiteral("utau")).toStdU16String());
        writeFile(pathIn("utau/resampler.exe"), "resampler");
        writeFile(pathIn("utau/wavtool.exe"), "wavtool");
        writeFile(pathIn("evil/tool.exe"), "evil");
        m_settings->setResampler(pathIn("utau/resampler.exe"));
        m_settings->setWavtool(pathIn("utau/wavtool.exe"));
    }

    void cleanup() {
        m_settings.reset();
        m_dir.reset();
    }

    void a_missing_synth_tool_is_never_trusted() {
        const auto missing = pathIn("nowhere/tool.exe");
        QVERIFY(!SynthToolTrust::exists(missing, m_utau));
        SynthToolTrust::trust(*m_settings, missing, m_utau);
        QVERIFY(!SynthToolTrust::isTrusted(*m_settings, missing, m_utau));
        QVERIFY(!SynthToolTrust::isAllowed(*m_settings, missing, m_utau));
        QVERIFY(!SynthToolTrust::isAllowed(*m_settings, QString(), m_utau));

        // Also if the settings name the same missing file
        m_settings->setResampler(missing);
        QVERIFY(!SynthToolTrust::isAllowed(*m_settings, missing, m_utau));
    }

    // The synth tools of the settings may run in either role, also written relative to the UTAU
    // folder.
    void the_synth_tools_of_the_settings_are_allowed_in_either_role() {
        QVERIFY(SynthToolTrust::isAllowed(*m_settings, pathIn("utau/resampler.exe"), m_utau));
        QVERIFY(SynthToolTrust::isAllowed(*m_settings, pathIn("utau/wavtool.exe"), m_utau));
        QVERIFY(SynthToolTrust::isAllowed(*m_settings, QStringLiteral("resampler.exe"), m_utau));
        QVERIFY(!SynthToolTrust::isAllowed(*m_settings, pathIn("evil/tool.exe"), m_utau));
    }

    // A trust record holds the content of the file, so that a changed file is no longer
    // trusted.
    void a_changed_file_is_no_longer_trusted() {
        const auto tool = pathIn("evil/tool.exe");
        SynthToolTrust::trust(*m_settings, tool, m_utau);
        QVERIFY(SynthToolTrust::isTrusted(*m_settings, tool, m_utau));
        QVERIFY(SynthToolTrust::isAllowed(*m_settings, tool, m_utau));

        writeFile(tool, "changed");
        QVERIFY(!SynthToolTrust::isTrusted(*m_settings, tool, m_utau));
        QVERIFY(!SynthToolTrust::isAllowed(*m_settings, tool, m_utau));
    }

    // A relative path that leaves the UTAU folder with .. can be trusted, and the question
    // shows the canonical path, so that the user sees the program that would run.
    void the_question_shows_the_canonical_path() {
        const auto relative = QStringLiteral("..\\evil\\tool.exe");
        QVERIFY(SynthToolTrust::exists(relative, m_utau));

        QString text;
        bool shown = false;
        answerNextBox(QMessageBox::No, &text, &shown);
        QVERIFY(!SynthToolTrust::ask(nullptr, *m_settings, {relative}, m_utau));
        QVERIFY(shown);
        QVERIFY2(text.contains(QDir::toNativeSeparators(
                     QFileInfo(pathIn("evil/tool.exe")).canonicalFilePath())),
                 qPrintable(text));
        QVERIFY(!text.contains(QStringLiteral("..")));
        QVERIFY(!SynthToolTrust::isTrusted(*m_settings, relative, m_utau));

        answerNextBox(QMessageBox::Yes, &text, &shown);
        QVERIFY(SynthToolTrust::ask(nullptr, *m_settings, {relative}, m_utau));
        QVERIFY(SynthToolTrust::isTrusted(*m_settings, relative, m_utau));
    }

    // Nothing is asked if a synth tool does not exist, or if every synth tool is allowed.
    void nothing_is_asked_without_need() {
        QString text;
        bool shown = false;
        answerNextBox(QMessageBox::Yes, &text, &shown);
        QVERIFY(!SynthToolTrust::ask(
            nullptr, *m_settings, {pathIn("evil/tool.exe"), pathIn("nowhere/tool.exe")}, m_utau));
        QCoreApplication::processEvents();
        QVERIFY(!shown);
        QVERIFY(!SynthToolTrust::isTrusted(*m_settings, pathIn("evil/tool.exe"), m_utau));

        answerNextBox(QMessageBox::Yes, &text, &shown);
        QVERIFY(SynthToolTrust::ask(nullptr, *m_settings,
                                    {pathIn("utau/resampler.exe"), pathIn("utau/wavtool.exe")},
                                    m_utau));
        QCoreApplication::processEvents();
        QVERIFY(!shown);
    }

    // A relative path written with backslashes, as UTAU writes it, names a file in a folder of
    // the UTAU directory on every platform.
    void a_relative_path_with_backslashes_is_found_in_the_utau_directory() {
        writeFile(pathIn("utau/tools/resampler.exe"), "resampler");
        const auto value = QStringLiteral("tools\\resampler.exe");
        QVERIFY(SynthToolTrust::exists(value, m_utau));
        QCOMPARE(SynthToolTrust::resolved(value, m_utau), m_utau / "tools" / "resampler.exe");
        QVERIFY(SynthToolTrust::samePath(value, pathIn("utau/tools/resampler.exe"), m_utau));
    }
};

QTEST_MAIN(test_SynthToolTrust)

#include "test_SynthToolTrust.moc"
