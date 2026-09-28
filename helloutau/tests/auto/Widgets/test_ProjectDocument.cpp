#include <filesystem>
#include <fstream>

#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Edit/ProjectRefs.h>

#include <helloutau/Widgets/ProjectDocument.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    // Answers with a fixed encoding, or declines when it has none, and records each question.
    class FixedSelector : public UstCharsetSelector {
    public:
        explicit FixedSelector(std::optional<QString> answer) : answer(std::move(answer)) {
        }

        std::optional<QString> selectCharset(const kit::UstDocument &ust,
                                             const fs::path &path) override {
            Q_UNUSED(ust);
            asked.push_back(path);
            return answer;
        }

        std::optional<QString> answer;
        QList<fs::path> asked;
    };

    fs::path pathIn(const QTemporaryDir &dir, const char *name) {
        return fs::path(dir.path().toStdU16String()) / name;
    }

    void writeBytes(const fs::path &path, const QByteArray &bytes) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(bytes.constData(), bytes.size());
    }

    QByteArray readBytes(const fs::path &path) {
        std::ifstream in(path, std::ios::binary);
        const std::string all((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
        return QByteArray(all.data(), qsizetype(all.size()));
    }

    // A UST as UTAU writes it, which states no encoding; the lyric is あ in Shift_JIS
    QByteArray unstatedUst() {
        return QByteArray("[#VERSION]\r\nUST Version1.2\r\n[#SETTING]\r\nTempo=120.00\r\n"
                          "Tracks=1\r\nVoiceDir=%VOICE%uta\r\nCacheDir=old.cache\r\n"
                          "Mode2=True\r\n[#0000]\r\nLength=480\r\nLyric=\x82\xa0\r\n"
                          "NoteNum=60\r\n[#TRACKEND]\r\n");
    }

    kit::Project oneNote() {
        kit::Note note;
        note.lyric = QStringLiteral("la");
        note.length = 480;
        note.noteNum = 60;
        kit::Track track;
        track.notes.push_back(note);
        kit::Project project;
        project.tracks.push_back(track);
        return project;
    }

    void rename(ProjectDocument &document, const QString &voiceDir) {
        auto tx = document.session()->transaction(QStringLiteral("rename"));
        kit::ProjectRef(document.session()).tracks().at(0).setVoiceDir(voiceDir);
        QVERIFY(tx.commit());
    }

}

class test_ProjectDocument : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_new_document_has_no_file_and_is_unmodified() {
        ProjectDocument document;
        QVERIFY(document.filePath().empty());
        QVERIFY(document.sourcePath().empty());
        QVERIFY(document.displayName().isEmpty());
        QVERIFY(!document.isModified());
        QCOMPARE(kit::ProjectRef(document.session()).tracks().size(), 1);
    }

    // Modified means a step other than the saved one, so undoing back to it clears the state.
    void the_modified_state_follows_the_step_of_the_session() {
        ProjectDocument document;
        QSignalSpy spy(&document, &ProjectDocument::modifiedChanged);

        rename(document, QStringLiteral("a"));
        QVERIFY(document.isModified());
        document.session()->undo();
        QVERIFY(!document.isModified());
        document.session()->redo();
        QVERIFY(document.isModified());
        QCOMPARE(spy.count(), 3);

        // Another step that leaves the document modified reports nothing.
        rename(document, QStringLiteral("b"));
        QCOMPARE(spy.count(), 3);
    }

    void a_usth_opens_with_its_file_and_saves_to_it() {
        QTemporaryDir dir;
        const auto path = pathIn(dir, "song.usth");
        kit::DiagnosticList diagnostics;
        QVERIFY(oneNote().save(path, diagnostics));

        const auto document = ProjectDocument::open(path, nullptr, diagnostics);
        QVERIFY(document);
        QCOMPARE(document->filePath(), path);
        QCOMPARE(document->displayName(), QStringLiteral("song.usth"));
        QVERIFY(!document->isModified());

        rename(*document, QStringLiteral("changed"));
        QVERIFY(document->save(diagnostics));
        QVERIFY(!document->isModified());
        QVERIFY(readBytes(path).contains("changed"));

        // Undoing past the saved step modifies the document again.
        document->session()->undo();
        QVERIFY(document->isModified());
    }

    // A UST is imported: the document has no file until it is saved as .usth.
    void a_ust_is_imported_without_a_file() {
        QTemporaryDir dir;
        const auto path = pathIn(dir, "song.ust");
        writeBytes(path, unstatedUst());

        FixedSelector selector(QStringLiteral("Shift_JIS"));
        kit::DiagnosticList diagnostics;
        const auto document = ProjectDocument::open(path, &selector, diagnostics);
        QVERIFY(document);
        QCOMPARE(selector.asked, QList<fs::path>{path});
        QVERIFY(document->filePath().empty());
        QCOMPARE(document->sourcePath(), path);
        QCOMPARE(document->displayName(), QStringLiteral("song.ust"));
        QVERIFY(!document->isModified());
        QCOMPARE(document->session()->snapshot().tracks[0].notes[0].lyric, QString::fromUtf8("あ"));
    }

    void a_ust_is_not_opened_when_the_user_declines_to_choose_an_encoding() {
        QTemporaryDir dir;
        const auto path = pathIn(dir, "song.ust");
        writeBytes(path, unstatedUst());

        FixedSelector selector(std::nullopt);
        kit::DiagnosticList diagnostics;
        QVERIFY(!ProjectDocument::open(path, &selector, diagnostics));
        QVERIFY(!kit::hasError(diagnostics));

        // Without anyone to ask, the file cannot be read, which is an error.
        QVERIFY(!ProjectDocument::open(path, nullptr, diagnostics));
        QVERIFY(kit::hasError(diagnostics));
    }

    void saving_as_gives_the_document_its_file() {
        QTemporaryDir dir;
        const auto source = pathIn(dir, "song.ust");
        writeBytes(source, unstatedUst());
        FixedSelector selector(QStringLiteral("Shift_JIS"));
        kit::DiagnosticList diagnostics;
        const auto document = ProjectDocument::open(source, &selector, diagnostics);
        QVERIFY(document);
        rename(*document, QStringLiteral("changed"));

        QSignalSpy spy(document.get(), &ProjectDocument::filePathChanged);
        const auto target = pathIn(dir, "saved.usth");
        QVERIFY(document->saveAs(target, diagnostics));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(document->filePath(), target);
        QCOMPARE(document->displayName(), QStringLiteral("saved.usth"));
        QVERIFY(!document->isModified());

        const auto again = kit::Project::open(target, diagnostics);
        QVERIFY(again);
        QCOMPARE(again->settings.cacheDir, QStringLiteral("saved.cache"));
    }

    // Exporting writes a UST without making it the file of the document.
    void exporting_leaves_the_document_as_it_was() {
        QTemporaryDir dir;
        ProjectDocument document;
        rename(document, QStringLiteral("changed"));

        const auto target = pathIn(dir, "export.ust");
        kit::DiagnosticList diagnostics;
        kit::UstDocument::ExportOptions options;
        options.charset = QStringLiteral("Shift_JIS");
        QVERIFY(document.exportUst(target, options, diagnostics));
        QVERIFY(document.filePath().empty());
        QVERIFY(document.isModified());

        const auto bytes = readBytes(target);
        QVERIFY(bytes.contains("CacheDir=export.cache\r\n"));
        QVERIFY(bytes.contains("VoiceDir=changed\r\n"));
    }

    void a_file_of_another_kind_is_refused() {
        QTemporaryDir dir;
        const auto path = pathIn(dir, "song.txt");
        writeBytes(path, "text");
        kit::DiagnosticList diagnostics;
        QVERIFY(!ProjectDocument::open(path, nullptr, diagnostics));
        QVERIFY(kit::hasError(diagnostics));
    }
};

QTEST_APPLESS_MAIN(test_ProjectDocument)

#include "test_ProjectDocument.moc"
