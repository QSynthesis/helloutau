#include <optional>

#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/Dialogs/ProjectPropertiesDialog.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    fs::path pathIn(const QTemporaryDir &dir, const char *name) {
        return fs::path(dir.path().toStdU16String()) / name;
    }

}

class test_ProjectPropertiesDialog : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private Q_SLOTS:
    // The dialog gives the fields that differ from the project.
    void the_properties_dialog_gives_what_differs() {
        kit::Project project;
        project.settings.name = QStringLiteral("song");
        project.settings.tempo = 125.125;
        project.settings.flags = QStringLiteral("g-5");
        project.tracks.push_back({});
        project.tracks[0].voiceDir = QStringLiteral("%VOICE%bank");

        // Settings without a UTAU directory, in which no %VOICE% value resolves
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        ProjectPropertiesDialog dialog(project, settings);
        QVERIFY(dialog.changes().isEmpty());
        QCOMPARE(dialog.voiceDirEdit()->text(), QStringLiteral("%VOICE%bank"));

        dialog.nameEdit()->setText(QStringLiteral("other"));
        dialog.voiceDirEdit()->setText(QStringLiteral("C:/voice/other"));
        dialog.mode2Box()->setChecked(!project.settings.mode2);
        auto changes = dialog.changes();
        QCOMPARE(changes.name, std::optional(QStringLiteral("other")));
        QCOMPARE(changes.voiceDir, std::optional(QStringLiteral("C:\\voice\\other")));
        QCOMPARE(changes.mode2, std::optional(!project.settings.mode2));
        QVERIFY(!changes.tempo && !changes.flags && !changes.wavtool && !changes.resampler &&
                !changes.outputFile);
    }

    void resetting_project_synth_tools_uses_native_separators() {
        AppSettings settings(m_dir.filePath(QStringLiteral("settings.json")));
        settings.setWavtool(QStringLiteral("C:/UTAU/tools/wavtool.exe"));
        settings.setResampler(QStringLiteral("C:/UTAU/tools/resampler.exe"));
        ProjectPropertiesDialog dialog(kit::Project(), settings);
        const auto reset =
            dialog.findChild<QPushButton *>(QStringLiteral("resetProjectSynthTools"));
        QVERIFY(reset);

        reset->click();
        QCOMPARE(dialog.wavtoolEdit()->text(), QDir::toNativeSeparators(settings.wavtool()));
        QCOMPARE(dialog.resamplerEdit()->text(), QDir::toNativeSeparators(settings.resampler()));
    }

    void project_properties_keep_the_initial_voice_bank() {
        const auto utau = pathIn(m_dir, "utau");
        fs::create_directories(utau / "voice" / "bank");
        AppSettings settings(m_dir.filePath(QStringLiteral("settings.json")));
        settings.setUtauDirectory(utau);

        kit::Project project;
        project.tracks.push_back({});
        project.tracks.first().voiceDir = QStringLiteral("%VOICE%bank");
        ProjectPropertiesDialog dialog(project, settings);

        QCOMPARE(dialog.voiceDirEdit()->text(), QStringLiteral("%VOICE%bank"));
        QVERIFY(!dialog.changes().voiceDir.has_value());
    }

    void project_properties_keep_a_voice_bank_subdirectory() {
        const auto utau = pathIn(m_dir, "utau");
        fs::create_directories(utau / "voice" / "bank" / "mid");
        AppSettings settings(m_dir.filePath(QStringLiteral("settings.json")));
        settings.setUtauDirectory(utau);
        kit::Project project;
        project.tracks.push_back({});
        project.tracks.first().voiceDir =
            QDir::toNativeSeparators(QStringLiteral("%VOICE%bank/mid"));
        ProjectPropertiesDialog dialog(project, settings);

        QCOMPARE(dialog.voiceDirEdit()->text(),
                 QDir::toNativeSeparators(QStringLiteral("%VOICE%bank/mid")));
        dialog.voiceDirEdit()->setText(QDir::toNativeSeparators(QStringLiteral("%VOICE%bank/mid")));
        QVERIFY(!dialog.changes().voiceDir.has_value());
        QVERIFY(!dialog.voiceDirEdit()->actions().isEmpty());
        QVERIFY(!dialog.voiceDirEdit()->actions().constFirst()->isVisible());
    }

    void project_properties_keep_an_absolute_voice_bank() {
        const auto utau = pathIn(m_dir, "utau");
        const auto bank = utau / "voice" / "bank";
        fs::create_directories(bank);
        fs::create_directories(utau / "voice" / "other");
        AppSettings settings(m_dir.filePath(QStringLiteral("settings.json")));
        settings.setUtauDirectory(utau);
        kit::Project project;
        project.tracks.push_back({});
        project.tracks.first().voiceDir =
            QDir::toNativeSeparators(QString::fromStdU16String(bank.u16string()));
        ProjectPropertiesDialog dialog(project, settings);

        QCOMPARE(dialog.voiceDirEdit()->text(),
                 QDir::toNativeSeparators(QString::fromStdU16String(bank.u16string())));
        QVERIFY(!dialog.changes().voiceDir.has_value());

        dialog.voiceDirEdit()->setText(QStringLiteral("%VOICE%other"));
        QCOMPARE(dialog.changes().voiceDir, std::optional(QStringLiteral("%VOICE%other")));
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_ProjectPropertiesDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_ProjectPropertiesDialog.moc"
