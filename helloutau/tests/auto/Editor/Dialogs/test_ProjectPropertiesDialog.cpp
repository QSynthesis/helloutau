#include <fstream>
#include <optional>

#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTranslator>
#include <QtGui/QAction>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
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
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_ProjectPropertiesDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_ProjectPropertiesDialog.moc"
