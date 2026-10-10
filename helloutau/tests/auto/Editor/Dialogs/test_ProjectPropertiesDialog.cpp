#include <fstream>
#include <optional>

#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/Dialogs/ProjectPropertiesDialog.h>
#include <helloutau/Editor/SynthToolTrust.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    fs::path pathIn(const QTemporaryDir &dir, const char *name) {
        return fs::path(dir.path().toStdU16String()) / name;
    }

    void writeFile(const fs::path &path) {
        std::ofstream(path, std::ios::binary) << "tool";
    }

    QString textOf(const fs::path &path) {
        return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
    }

    // Returns the sorted colors of the visible labels of dialog whose text is a colored span that
    // is bold if bold is true.
    QStringList colorsOf(const QDialog &dialog, bool bold) {
        static const QRegularExpression span(
            QStringLiteral("^<span style=\"color:(#[0-9a-f]{6});( font-weight:bold;)?\">"));
        QStringList colors;
        for (const auto label : dialog.findChildren<QLabel *>()) {
            const auto match = span.match(label->text());
            if (match.hasMatch() && match.hasCaptured(2) == bold && label->isVisibleTo(&dialog)) {
                colors.push_back(match.captured(1));
            }
        }
        colors.sort();
        return colors;
    }

    // Translates every text of ProjectPropertiesDialog to a text with the characters that rich
    // text requires to be escaped.
    class MarkupTranslator : public QTranslator {
    public:
        QString translate(const char *context, const char *sourceText, const char *disambiguation,
                          int n) const override {
            Q_UNUSED(sourceText)
            Q_UNUSED(disambiguation)
            Q_UNUSED(n)
            return qstrcmp(context, "hello::daw::ProjectPropertiesDialog") == 0
                       ? QStringLiteral("a < b & c")
                       : QString();
        }

        bool isEmpty() const override {
            return false;
        }
    };

    // The synth tools in a UTAU directory. The settings use the wavtool and the resampler, and
    // the first and the second are other synth tools.
    struct SynthTools {
        fs::path utau;
        QString wavtool;
        QString resampler;
        QString first;
        QString second;
    };

    SynthTools synthToolsIn(const QTemporaryDir &dir, AppSettings &settings) {
        SynthTools tools;
        tools.utau = pathIn(dir, "utau");
        fs::create_directories(tools.utau);
        for (const auto name : {"wavtool.exe", "resampler.exe", "first.exe", "second.exe"}) {
            writeFile(tools.utau / name);
        }
        tools.wavtool = textOf(tools.utau / "wavtool.exe");
        tools.resampler = textOf(tools.utau / "resampler.exe");
        tools.first = textOf(tools.utau / "first.exe");
        tools.second = textOf(tools.utau / "second.exe");
        settings.setUtauDirectory(tools.utau);
        settings.setWavtool(tools.wavtool);
        settings.setResampler(tools.resampler);
        return tools;
    }

    // Creates the voice bank bank in the voice folder of settings, and resampler.exe and
    // tools\wavtool.exe in a UTAU directory, which settings uses. Returns the UTAU directory.
    fs::path utauWithSynthToolsIn(const QTemporaryDir &dir, AppSettings &settings) {
        fs::create_directories(settings.voiceFolder() / "bank");
        const auto utau = pathIn(dir, "utau");
        fs::create_directories(utau / "tools");
        writeFile(utau / "resampler.exe");
        writeFile(utau / "tools" / "wavtool.exe");
        settings.setUtauDirectory(utau);
        return utau;
    }

    // Clicks the OK button of dialog. Returns whether a message box was shown, after closing the
    // message box.
    bool warnsOnOk(QDialog &dialog) {
        bool warned = false;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, [&warned] {
            const auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box) {
                warned = true;
                box->button(QMessageBox::Ok)->click();
            }
        });
        timer.start(0);
        dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        return warned;
    }

    // Creates Alpha and shared in the voice folder of settings, and beta and shared in the voice
    // folder of a UTAU directory, which settings uses. Returns the UTAU directory.
    fs::path voiceFoldersIn(const QTemporaryDir &dir, AppSettings &settings) {
        const auto utau = pathIn(dir, "utau");
        for (const auto &folder :
             {settings.voiceFolder() / "Alpha", settings.voiceFolder() / "shared",
              utau / "voice" / "beta", utau / "voice" / "shared"}) {
            fs::create_directories(folder);
        }
        settings.setUtauDirectory(utau);
        return utau;
    }

    // Returns the label of dialog that shows the resolved voice folder, the only label whose text
    // is selectable.
    QLabel *resolvedLabelOf(const QDialog &dialog) {
        for (const auto label : dialog.findChildren<QLabel *>()) {
            if (label->textInteractionFlags().testFlag(Qt::TextSelectableByMouse)) {
                return label;
            }
        }
        return nullptr;
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

    // The same synth tools as the settings, the swapped synth tools of the settings and trusted
    // synth tools are each shown bold in the trusted color, without the warning.
    void allowed_synth_tools_are_bold_in_the_trusted_color() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        const auto tools = synthToolsIn(dir, settings);
        kit::Project project;
        project.settings.wavtool = QStringLiteral("wavtool.exe");
        project.settings.resampler = tools.resampler;
        ProjectPropertiesDialog dialog(project, settings);
        const auto trusted = dialog.trustedColor().name();
        QCOMPARE(colorsOf(dialog, true), (QStringList{trusted, trusted}));
        QVERIFY(colorsOf(dialog, false).isEmpty());

        dialog.wavtoolEdit()->setText(tools.resampler);
        dialog.resamplerEdit()->setText(tools.wavtool);
        QCOMPARE(colorsOf(dialog, true), (QStringList{trusted, trusted}));
        QVERIFY(colorsOf(dialog, false).isEmpty());

        SynthToolTrust::trust(settings, tools.first, tools.utau);
        SynthToolTrust::trust(settings, tools.second, tools.utau);
        dialog.wavtoolEdit()->setText(tools.first);
        dialog.resamplerEdit()->setText(tools.second);
        QCOMPARE(colorsOf(dialog, true), (QStringList{trusted, trusted}));
        QVERIFY(colorsOf(dialog, false).isEmpty());
    }

    // An untrusted synth tool is shown bold in the untrusted color, and the warning is shown in
    // the untrusted color without bold.
    void an_untrusted_synth_tool_is_bold_in_the_untrusted_color_with_a_warning() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        const auto tools = synthToolsIn(dir, settings);
        kit::Project project;
        project.settings.wavtool = tools.first;
        project.settings.resampler = tools.resampler;
        ProjectPropertiesDialog dialog(project, settings);
        const auto trusted = dialog.trustedColor().name();
        const auto untrusted = dialog.untrustedColor().name();
        QCOMPARE(colorsOf(dialog, true), (QStringList{trusted, untrusted}));
        QCOMPARE(colorsOf(dialog, false), QStringList{untrusted});

        dialog.resamplerEdit()->setText(textOf(tools.utau / "missing.exe"));
        QCOMPARE(colorsOf(dialog, true), (QStringList{untrusted, untrusted}));
        QCOMPARE(colorsOf(dialog, false), QStringList{untrusted});

        dialog.wavtoolEdit()->setText(tools.wavtool);
        dialog.resamplerEdit()->setText(tools.resampler);
        QVERIFY(colorsOf(dialog, false).isEmpty());
    }

    void the_states_follow_the_color_properties() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        const auto tools = synthToolsIn(dir, settings);
        kit::Project project;
        project.settings.wavtool = tools.wavtool;
        project.settings.resampler = tools.first;
        ProjectPropertiesDialog dialog(project, settings);

        dialog.setTrustedColor(QColor(0x12, 0x34, 0x56));
        QCOMPARE(dialog.trustedColor(), QColor(0x12, 0x34, 0x56));
        QCOMPARE(colorsOf(dialog, true),
                 (QStringList{QStringLiteral("#123456"), dialog.untrustedColor().name()}));
        dialog.setUntrustedColor(QColor(0xab, 0xcd, 0xef));
        QCOMPARE(dialog.untrustedColor(), QColor(0xab, 0xcd, 0xef));
        QCOMPARE(colorsOf(dialog, true),
                 (QStringList{QStringLiteral("#123456"), QStringLiteral("#abcdef")}));
        QCOMPARE(colorsOf(dialog, false), QStringList{QStringLiteral("#abcdef")});
    }

    // A translated state text is escaped for rich text.
    void the_state_texts_are_escaped() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        const auto tools = synthToolsIn(dir, settings);
        kit::Project project;
        project.settings.wavtool = tools.first;
        MarkupTranslator translator;
        QCoreApplication::installTranslator(&translator);
        ProjectPropertiesDialog dialog(project, settings);
        QCoreApplication::removeTranslator(&translator);

        int spans = 0;
        for (const auto label : dialog.findChildren<QLabel *>()) {
            if (label->text().startsWith(QStringLiteral("<span"))) {
                QVERIFY(label->text().endsWith(QStringLiteral(">a &lt; b &amp; c</span>")));
                ++spans;
            }
        }
        QCOMPARE(spans, 3);
    }

    // Without a change, the changes are empty even if the voice folder and the synth tools are
    // not normalized.
    void unchanged_fields_are_not_normalized() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        const auto utau = utauWithSynthToolsIn(dir, settings);
        kit::Project project;
        project.tracks.push_back({});
        project.tracks[0].voiceDir = textOf(settings.voiceFolder() / "bank");
        project.settings.wavtool = textOf(utau / "tools" / ".." / "tools" / "wavtool.exe");
        project.settings.resampler = textOf(utau / "resampler.exe");
        ProjectPropertiesDialog dialog(project, settings);
        QVERIFY(dialog.changes().isEmpty());
    }

    // A change of another field normalizes the voice folder and the synth tools as well.
    void a_change_normalizes_the_voice_folder_and_the_synth_tools() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        const auto utau = utauWithSynthToolsIn(dir, settings);
        kit::Project project;
        project.tracks.push_back({});
        project.tracks[0].voiceDir = textOf(settings.voiceFolder() / "bank");
        project.settings.wavtool = textOf(utau / "tools" / ".." / "tools" / "wavtool.exe");
        project.settings.resampler = textOf(utau / "resampler.exe");
        ProjectPropertiesDialog dialog(project, settings);

        dialog.nameEdit()->setText(QStringLiteral("other"));
        const auto changes = dialog.changes();
        QCOMPARE(changes.name, std::optional(QStringLiteral("other")));
        QCOMPARE(changes.voiceDir, std::optional(QStringLiteral("%VOICE%bank")));
        QCOMPARE(changes.wavtool, std::optional(QStringLiteral("tools\\wavtool.exe")));
        QCOMPARE(changes.resampler, std::optional(QStringLiteral("resampler.exe")));
        QVERIFY(!changes.tempo && !changes.flags && !changes.outputFile && !changes.mode2);
    }

    // A field whose normalized value equals the value of the project is left out of the changes.
    // A %VOICE% value, a relative voice folder, a synth tool relative to the UTAU directory and
    // an absolute synth tool outside the UTAU directory are normalized.
    void normalized_values_are_left_out_of_the_changes() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        utauWithSynthToolsIn(dir, settings);
        const auto outside = pathIn(dir, "resampler.exe");
        writeFile(outside);
        kit::Project project;
        project.tracks.push_back({});
        project.settings.wavtool = QStringLiteral("tools\\wavtool.exe");
        project.settings.resampler = textOf(outside);
        for (const auto &voiceDir : {QStringLiteral("%VOICE%bank"), QStringLiteral("bank")}) {
            project.tracks[0].voiceDir = voiceDir;
            ProjectPropertiesDialog dialog(project, settings);
            dialog.nameEdit()->setText(QStringLiteral("other"));
            const auto changes = dialog.changes();
            QVERIFY(changes.name);
            QVERIFY(!changes.voiceDir);
            QVERIFY(!changes.wavtool);
            QVERIFY(!changes.resampler);
        }
    }

    // The normalized paths use slashes if they begin with a slash, or else backslashes.
    void normalized_paths_use_the_separators_of_a_saved_project() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        kit::Project project;
        project.tracks.push_back({});
        ProjectPropertiesDialog dialog(project, settings);

        dialog.voiceDirEdit()->setText(QStringLiteral("/voice\\bank"));
        dialog.wavtoolEdit()->setText(QStringLiteral("/tools\\wavtool.exe"));
        dialog.resamplerEdit()->setText(QStringLiteral("tools/resampler.exe"));
        const auto changes = dialog.changes();
        QCOMPARE(changes.voiceDir, std::optional(QStringLiteral("/voice/bank")));
        QCOMPARE(changes.wavtool, std::optional(QStringLiteral("/tools/wavtool.exe")));
        QCOMPARE(changes.resampler, std::optional(QStringLiteral("tools\\resampler.exe")));
    }

    // Without a change, the dialog is accepted even if a path is invalid. A change requires the
    // voice folder and the synth tools to be valid.
    void a_change_requires_valid_paths() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        utauWithSynthToolsIn(dir, settings);
        kit::Project project;
        project.tracks.push_back({});
        project.tracks[0].voiceDir = QStringLiteral("%VOICE%missing");
        project.settings.wavtool = QStringLiteral("missing.exe");
        project.settings.resampler = QStringLiteral("missing.exe");
        {
            ProjectPropertiesDialog dialog(project, settings);
            QVERIFY(!warnsOnOk(dialog));
            QCOMPARE(dialog.result(), int(QDialog::Accepted));
        }

        project.tracks[0].voiceDir = QStringLiteral("%VOICE%bank");
        project.settings.wavtool = QStringLiteral("tools\\wavtool.exe");
        project.settings.resampler = QStringLiteral("resampler.exe");
        ProjectPropertiesDialog dialog(project, settings);
        dialog.nameEdit()->setText(QStringLiteral("other"));
        for (const auto edit :
             {dialog.voiceDirEdit(), dialog.wavtoolEdit(), dialog.resamplerEdit()}) {
            const auto valid = edit->text();
            edit->setText(QStringLiteral("missing"));
            QVERIFY(warnsOnOk(dialog));
            QCOMPARE(dialog.result(), int(QDialog::Rejected));
            edit->setText(valid);
        }
        QVERIFY(!warnsOnOk(dialog));
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }

    // The items are the values that UTAU writes for the folders of the voice folders. A folder
    // hidden by a folder of the same name in a voice folder of higher priority is an absolute
    // path.
    void the_voice_folder_items_are_the_values_that_utau_writes() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        const auto utau = voiceFoldersIn(dir, settings);
        kit::Project project;
        project.tracks.push_back({});
        ProjectPropertiesDialog dialog(project, settings);
        const auto box = dialog.findChild<QComboBox *>();
        QVERIFY(box);
        QStringList items;
        for (int i = 0; i < box->count(); ++i) {
            items.push_back(box->itemText(i));
        }
        QCOMPARE(items,
                 (QStringList{QStringLiteral("%VOICE%Alpha"), QStringLiteral("%VOICE%shared"),
                              QStringLiteral("%VOICE%beta"), textOf(utau / "voice" / "shared")}));
    }

    // A typed name is a relative voice folder, not a %VOICE% value.
    void a_typed_name_is_a_relative_voice_folder() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        voiceFoldersIn(dir, settings);
        kit::Project project;
        project.tracks.push_back({});
        project.tracks[0].voiceDir = QStringLiteral("%VOICE%beta");
        ProjectPropertiesDialog dialog(project, settings);

        dialog.voiceDirEdit()->setText(QStringLiteral("Alpha"));
        QCOMPARE(dialog.changes().voiceDir, std::optional(QStringLiteral("Alpha")));
    }

    void the_voice_folder_completion_matches_any_part_ignoring_case() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        voiceFoldersIn(dir, settings);
        kit::Project project;
        project.tracks.push_back({});
        ProjectPropertiesDialog dialog(project, settings);
        const auto completer = dialog.findChild<QComboBox *>()->completer();
        QVERIFY(completer);

        completer->setCompletionPrefix(QStringLiteral("LPH"));
        QCOMPARE(completer->completionCount(), 1);
        QCOMPARE(completer->currentCompletion(), QStringLiteral("%VOICE%Alpha"));
    }

    // The resolved voice folder is shown below the box, and is hidden if the voice folder does
    // not resolve.
    void the_resolved_voice_folder_is_shown_if_resolved() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        voiceFoldersIn(dir, settings);
        kit::Project project;
        project.tracks.push_back({});
        project.tracks[0].voiceDir = QStringLiteral("%VOICE%shared");
        ProjectPropertiesDialog dialog(project, settings);
        const auto label = resolvedLabelOf(dialog);
        QVERIFY(label);
        QVERIFY(label->isVisibleTo(&dialog));
        QVERIFY(label->text().endsWith(textOf(settings.voiceFolder() / "shared")));

        dialog.voiceDirEdit()->setText(QStringLiteral("%VOICE%beta"));
        QVERIFY(label->isVisibleTo(&dialog));
        QVERIFY(label->text().endsWith(textOf(pathIn(dir, "utau") / "voice" / "beta")));

        dialog.voiceDirEdit()->clear();
        QVERIFY(!label->isVisibleTo(&dialog));
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
