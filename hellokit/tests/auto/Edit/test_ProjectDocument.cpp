#include <filesystem>
#include <fstream>

#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Document/UstDocument.h>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/VoiceBank/VoiceBank.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    // Answers with a fixed encoding, or declines when it has none, and records each question.
    class FixedSelector : public UstCharsetSelector {
    public:
        explicit FixedSelector(std::optional<QString> answer) : answer(std::move(answer)) {
        }

        std::optional<QString> selectCharset(const UstDocument &ust,
                                             const fs::path &path) override {
            Q_UNUSED(ust);
            asked.push_back(path);
            return answer;
        }

        std::optional<QString> answer;
        QList<fs::path> asked;
    };

    // Answers every directory with a fixed encoding, or declines when it has none, and records
    // each directory asked about.
    class FixedVoiceBankSelector : public VoiceBankCharsetSelector {
    public:
        explicit FixedVoiceBankSelector(std::optional<QString> answer) : answer(std::move(answer)) {
        }

        std::optional<QString> selectCharset(const VoiceBankDirectorySource &directory,
                                             DiagnosticList &diagnostics) override {
            Q_UNUSED(diagnostics);
            asked.push_back(directory.path);
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

    Project oneNote() {
        Note note;
        note.lyric = QStringLiteral("la");
        note.length = 480;
        note.noteNum = 60;
        Track track;
        track.notes.push_back(note);
        Project project;
        project.tracks.push_back(track);
        return project;
    }

    // The oto.ini of a voice bank as UTAU users write it, which states no encoding; the alias is
    // あ in Shift_JIS
    const QByteArray unstatedOto("a.wav=\x82\xa0,0,0,0,0,0\r\n");

    // A UTAU folder under \a dir with a voice bank "bank" in its voice folder, holding
    // unstatedOto; returns the UTAU folder.
    fs::path utauWithVoiceBank(const QTemporaryDir &dir) {
        const auto utau = pathIn(dir, "utau");
        const auto bank = utau / "voice" / "bank";
        fs::create_directories(bank);
        writeBytes(bank / "oto.ini", unstatedOto);
        writeBytes(bank / "a.wav", {});
        return utau;
    }

    // A document whose track sings with \a voiceDir
    std::unique_ptr<ProjectDocument> singingWith(const QTemporaryDir &dir,
                                                 const QString &voiceDir) {
        auto project = oneNote();
        project.tracks[0].voiceDir = voiceDir;
        const auto path = pathIn(dir, "song.usth");
        DiagnosticList diagnostics;
        if (!project.save(path, diagnostics)) {
            return nullptr;
        }
        return ProjectDocument::open(path, nullptr, diagnostics);
    }

    void rename(ProjectDocument &document, const QString &voiceDir) {
        auto tx = document.session()->transaction(QStringLiteral("rename"));
        ProjectRef(document.session()).tracks().at(0).setVoiceDir(voiceDir);
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
        QCOMPARE(ProjectRef(document.session()).tracks().size(), 1);
    }

    // A voice bank given as saved elsewhere is no edit of the project.
    void a_voice_bank_is_set_without_an_edit() {
        ProjectDocument document;
        QSignalSpy changed(&document, &ProjectDocument::voiceBankChanged);
        const auto bank = std::make_shared<const VoiceBank>(
            fs::path("bank"), QList<VoiceBankDirectory>{{}}, QList<VoiceSample>{});
        document.setVoiceBank(bank);
        QCOMPARE(document.voiceBank(), bank);
        QCOMPARE(changed.count(), 1);
        document.setVoiceBank(bank);
        QCOMPARE(changed.count(), 1);
        QVERIFY(!document.isModified());
        QVERIFY(!document.session()->canUndo());
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

    // A new step that takes the number of the saved one, after an undo, is not the saved state.
    void a_step_that_replaces_the_saved_one_is_modified() {
        ProjectDocument document;
        rename(document, QStringLiteral("a"));
        QTemporaryDir dir;
        DiagnosticList diagnostics;
        QVERIFY(document.saveAs(fs::path(dir.path().toStdU16String()) / "a.usth", diagnostics));
        QVERIFY(!document.isModified());

        document.session()->undo();
        QVERIFY(document.isModified());
        rename(document, QStringLiteral("b"));
        QCOMPARE(document.session()->currentStep(), 1);
        QVERIFY(document.isModified());
        document.session()->undo();
        QVERIFY(document.isModified());
    }

    void a_usth_opens_with_its_file_and_saves_to_it() {
        QTemporaryDir dir;
        const auto path = pathIn(dir, "song.usth");
        DiagnosticList diagnostics;
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

    // The last point of every note is at the pitch of the note once opened, as the editor writes
    // points, and opening does not modify the document.
    void the_last_point_ends_at_the_pitch_of_the_note_when_opened() {
        auto project = oneNote();
        PortamentoPoint first;
        first.x = -40;
        first.y = 20;
        PortamentoPoint last;
        last.x = 40;
        last.y = 30;
        project.tracks[0].notes[0].portamento = {first, last};

        QTemporaryDir dir;
        DiagnosticList diagnostics;
        const auto usth = pathIn(dir, "song.usth");
        QVERIFY(project.save(usth, diagnostics));
        const auto ust = pathIn(dir, "song.ust");
        UstDocument::ExportOptions options;
        options.charset = QStringLiteral("UTF-8");
        options.file = ust;
        const auto exported = UstDocument::fromProject(project, options, diagnostics);
        QVERIFY(exported && exported->save(ust, diagnostics));

        FixedSelector selector(QStringLiteral("UTF-8"));
        for (const auto &path : {usth, ust}) {
            const auto document = ProjectDocument::open(path, &selector, diagnostics);
            QVERIFY(document);
            const auto points = document->session()->snapshot().tracks[0].notes[0].portamento;
            QCOMPARE(points.size(), 2);
            QCOMPARE(points[0].y, 20.0);
            QCOMPARE(points[1].y, 0.0);
            QVERIFY(!document->isModified());
            QVERIFY(!document->session()->canUndo());
        }
    }

    // A UST is imported: the document has no file until it is saved as .usth.
    void a_ust_is_imported_without_a_file() {
        QTemporaryDir dir;
        const auto path = pathIn(dir, "song.ust");
        writeBytes(path, unstatedUst());

        FixedSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
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
        DiagnosticList diagnostics;
        QVERIFY(!ProjectDocument::open(path, &selector, diagnostics));
        QVERIFY(!hasError(diagnostics));

        // Without anyone to ask, the file cannot be read, which is an error.
        QVERIFY(!ProjectDocument::open(path, nullptr, diagnostics));
        QVERIFY(hasError(diagnostics));
    }

    void saving_as_gives_the_document_its_file() {
        QTemporaryDir dir;
        const auto source = pathIn(dir, "song.ust");
        writeBytes(source, unstatedUst());
        FixedSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
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

        const auto again = Project::open(target, diagnostics);
        QVERIFY(again);
        QCOMPARE(again->settings.cacheDir, QStringLiteral("saved.cache"));
    }

    // Exporting writes a UST without making it the file of the document.
    void exporting_leaves_the_document_as_it_was() {
        QTemporaryDir dir;
        ProjectDocument document;
        rename(document, QStringLiteral("changed"));

        const auto target = pathIn(dir, "export.ust");
        DiagnosticList diagnostics;
        UstDocument::ExportOptions options;
        options.charset = QStringLiteral("Shift_JIS");
        QVERIFY(document.exportUst(target, options, diagnostics));
        QVERIFY(document.filePath().empty());
        QVERIFY(document.isModified());

        const auto bytes = readBytes(target);
        QVERIFY(bytes.contains("CacheDir=export.cache\r\n"));
        QVERIFY(bytes.contains("VoiceDir=changed\r\n"));
    }

    // The encoding chosen for a directory is recorded at once, so the next load asks nothing,
    // and no other file of the voice bank is written.
    void the_voice_bank_is_read_and_the_chosen_encoding_recorded() {
        QTemporaryDir dir;
        const auto utau = utauWithVoiceBank(dir);
        const auto document = singingWith(dir, QStringLiteral("%VOICE%bank"));
        QVERIFY(document);
        QVERIFY(!document->voiceBank());

        QSignalSpy spy(document.get(), &ProjectDocument::voiceBankChanged);
        FixedVoiceBankSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        QVERIFY(document->loadVoiceBank(VoiceLocations::ofUtau(utau), &selector, diagnostics));
        QVERIFY(diagnostics.empty());
        QCOMPARE(selector.asked, QList<fs::path>{fs::path()});
        QCOMPARE(spy.count(), 1);
        const auto bank = document->voiceBank();
        QVERIFY(bank);
        QVERIFY(bank->find(60, QString::fromUtf8("あ")));

        const auto folder = utau / "voice" / "bank";
        QVERIFY(fs::is_regular_file(folder / "hello-config.json"));
        QCOMPARE(readBytes(folder / "oto.ini"), unstatedOto);

        FixedVoiceBankSelector again(QStringLiteral("GBK"));
        QVERIFY(document->loadVoiceBank(VoiceLocations::ofUtau(utau), &again, diagnostics));
        QVERIFY(again.asked.isEmpty());
        QVERIFY(document->voiceBank()->find(60, QString::fromUtf8("あ")));
        QCOMPARE(spy.count(), 2);
    }

    // A directory the user declined is left out, and nothing is recorded for it.
    void a_declined_directory_is_not_recorded() {
        QTemporaryDir dir;
        const auto utau = utauWithVoiceBank(dir);
        const auto document = singingWith(dir, QStringLiteral("%VOICE%bank"));
        QVERIFY(document);

        FixedVoiceBankSelector selector(std::nullopt);
        DiagnosticList diagnostics;
        QVERIFY(document->loadVoiceBank(VoiceLocations::ofUtau(utau), &selector, diagnostics));
        QVERIFY(!document->voiceBank()->find(60, QString::fromUtf8("あ")));
        QVERIFY(!fs::exists(utau / "voice" / "bank" / "hello-config.json"));
    }

    // The voice bank is read even if the encoding cannot be recorded, which is only a warning.
    void failing_to_record_the_encoding_is_a_warning() {
        QTemporaryDir dir;
        const auto utau = utauWithVoiceBank(dir);
        fs::create_directory(utau / "voice" / "bank" / "hello-config.json");
        const auto document = singingWith(dir, QStringLiteral("%VOICE%bank"));
        QVERIFY(document);

        FixedVoiceBankSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        QVERIFY(document->loadVoiceBank(VoiceLocations::ofUtau(utau), &selector, diagnostics));
        QVERIFY(document->voiceBank()->find(60, QString::fromUtf8("あ")));
        QVERIFY(!diagnostics.empty());
        QVERIFY(!hasError(diagnostics));
    }

    void a_voice_bank_that_cannot_be_found_is_not_read() {
        QTemporaryDir dir;
        const auto utau = utauWithVoiceBank(dir);
        FixedVoiceBankSelector selector(QStringLiteral("Shift_JIS"));

        // Without a UTAU folder, a voice bank inside it cannot be located.
        auto document = singingWith(dir, QStringLiteral("%VOICE%bank"));
        QVERIFY(document);
        DiagnosticList diagnostics;
        QVERIFY(!document->loadVoiceBank({}, &selector, diagnostics));
        QVERIFY(!document->voiceBank());
        QVERIFY(!diagnostics.empty());

        diagnostics.clear();
        document = singingWith(dir, QStringLiteral("%VOICE%missing"));
        QVERIFY(document);
        QVERIFY(!document->loadVoiceBank(VoiceLocations::ofUtau(utau), &selector, diagnostics));
        QVERIFY(!document->voiceBank());
        QVERIFY(hasError(diagnostics));

        // A track that names no voice bank is not an error.
        diagnostics.clear();
        document = singingWith(dir, QString());
        QVERIFY(document);
        QVERIFY(!document->loadVoiceBank(VoiceLocations::ofUtau(utau), &selector, diagnostics));
        QVERIFY(diagnostics.empty());
        QVERIFY(selector.asked.isEmpty());
    }

    void a_file_of_another_kind_is_refused() {
        QTemporaryDir dir;
        const auto path = pathIn(dir, "song.txt");
        writeBytes(path, "text");
        DiagnosticList diagnostics;
        QVERIFY(!ProjectDocument::open(path, nullptr, diagnostics));
        QVERIFY(hasError(diagnostics));
    }
};

QTEST_APPLESS_MAIN(test_ProjectDocument)

#include "test_ProjectDocument.moc"
