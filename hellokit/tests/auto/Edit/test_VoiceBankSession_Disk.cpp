#include <functional>
#include <map>
#include <memory>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Edit/VoiceBankEdits.h>
#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/Edit/VoiceBankSession.h>
#include <hellokit/VoiceBank/BuiltinFrequencyFormats.h>
#include <hellokit/VoiceBank/FrequencyFormats.h>

#include "VoiceBankSamples.h"

using namespace hello::kit;

namespace fs = std::filesystem;

// Saving and reading from disk through a session, see the section on changes on disk in
// docs/Editing.md.
class test_VoiceBankSession_Disk : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<VoiceBankSession> m_session;

    QString pathOf(const QString &relative) const {
        return m_dir->path() + QLatin1Char('/') + relative;
    }

    void write(const QString &relative, const QByteArray &bytes) {
        QVERIFY(writeSampleFile(m_dir->path(), relative, bytes));
        // Dated back, so that the stamp alone shows the change.
        QFile file(pathOf(relative));
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(-3600),
                                 QFileDevice::FileModificationTime));
    }

    QByteArray read(const QString &relative) const {
        QFile file(pathOf(relative));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
    }

    // Opens a voice bank of a root in GBK and the subdirectories sub in UTF-8 and left, whose
    // encoding the user did not select.
    void open() {
        write(QStringLiteral("oto.ini"), "a.wav=a,41.0,2,3,4,5\r\nb.wav=b,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("sub/oto.ini"), "x.wav=x,1,2,3,4,5\r\n");
        write(QStringLiteral("left/oto.ini"), "z.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        DirectorySelector selector({
            {fs::path(),      QStringLiteral("GBK")  },
            {fs::path("sub"), QStringLiteral("UTF-8")},
        });
        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(fs::path(m_dir->path().toStdU16String()),
                                                     &selector, diagnostics);
        QVERIFY(opened);
        m_session = VoiceBankSession::create(std::move(*opened), diagnostics);
        QVERIFY(m_session);
    }

    VoiceDirectoryListRef directories() const {
        return VoiceBankRef(m_session.get()).directories();
    }

    int indexOf(const fs::path &path) const {
        const auto list = directories();
        for (int i = 0; i < list.size(); ++i) {
            if (list.at(i).path() == path) {
                return i;
            }
        }
        return -1;
    }

    using Times = std::map<QString, fs::file_time_type>;

    /// The modification times of the text files of the fixture, by which a test finds the files
    /// that a save wrote. A missing file has the default time.
    Times times() const {
        Times out;
        for (const auto relative :
             {"oto.ini", "sub/oto.ini", "hello-config.json", "sub/hello-config.json",
              "character.txt", "prefix.map", "readme.txt"}) {
            const auto name = QString::fromLatin1(relative);
            std::error_code error;
            const auto time = fs::last_write_time(fs::path(pathOf(name).toStdU16String()), error);
            out[name] = error ? fs::file_time_type() : time;
        }
        return out;
    }

    /// Returns the files of times() whose modification time differs from \a before .
    QStringList writtenSince(const Times &before) const {
        QStringList written;
        for (const auto &[name, time] : times()) {
            if (before.at(name) != time) {
                written.push_back(name);
            }
        }
        return written;
    }

    /// Sets the offset of the first entry of the directory at \a path in one step.
    void editFirstEntry(const fs::path &path, double offset) {
        auto transaction = m_session->transaction(QStringLiteral("Edit"));
        directories().at(indexOf(path)).otoEntries().at(0).setOffset(offset);
        QVERIFY(transaction.commit());
    }

    void save() {
        DiagnosticList diagnostics;
        QVERIFY2(m_session->save(diagnostics),
                 diagnostics.isEmpty() ? "" : qPrintable(diagnostics.first().message));
    }

    VoiceBankChanges reload() {
        DiagnosticList diagnostics;
        return m_session->reloadFromDisk(m_session->checkDisk(), nullptr, diagnostics);
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        open();
    }

    void cleanup() {
        m_session.reset();
        m_dir.reset();
    }

    // The tree holds the logical names, and the snapshot takes the names on disk from the file
    // system state. あ.wav and い.wav are on disk as 偁.wav and 偄.wav.
    void the_snapshot_finds_the_audio_files_under_their_names_on_disk() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto a = QString::fromUtf8("偁.wav");
        const auto i = QString::fromUtf8("偄.wav");
        VoiceBankConfiguration config;
        config.charset = QStringLiteral("Shift_JIS");
        VoiceBankConfiguration::FileNameEncoding record;
        record.original = QStringLiteral("Shift_JIS");
        record.misdecodedAs = QStringLiteral("GBK");
        const auto u = QString::fromUtf8("偆");
        record.files = VoiceBankConfiguration::FileNameEncoding::Files{
            {a, i, u},
            {QString::fromUtf8("が.wav")}
        };
        config.fileNameEncoding = record;
        QVERIFY(writeSampleFile(dir.path(), QStringLiteral("hello-config.json"), config.toJson()));
        QVERIFY(writeSampleFile(dir.path(), QStringLiteral("oto.ini"),
                                QByteArray("\x82\xa0.wav=a,1,2,3,4,5\r\n")));
        QVERIFY(writeSampleFile(dir.path(), a, "RIFF"));
        QVERIFY(writeSampleFile(dir.path(), i, "RIFF"));
        QVERIFY(writeSampleFile(dir.path(), u + QStringLiteral("/b.wav"), "RIFF"));
        QVERIFY(writeSampleFile(dir.path(), QString::fromUtf8("新.wav"), "RIFF"));

        const fs::path root(dir.path().toStdU16String());
        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root, nullptr, diagnostics);
        QVERIFY(opened);
        const auto session = VoiceBankSession::create(std::move(*opened), diagnostics);
        QVERIFY(session);
        const auto bank = session->snapshot();
        QCOMPARE(bank.samples().size(), 4);
        QCOMPARE(bank.samples().at(0).fileName, QString::fromUtf8("あ.wav"));
        QCOMPARE(bank.samples().at(0).path, root / a.toStdU16String());
        QCOMPARE(bank.samples().at(1).fileName, QString::fromUtf8("い.wav"));
        QCOMPARE(bank.samples().at(1).path, root / i.toStdU16String());

        // The names that the session gives follow the records of its tree.
        QVERIFY(session->audioFiles({}).contains(QString::fromUtf8("い.wav")));
        QCOMPARE(session->audioPathOf({}, QString::fromUtf8("い.wav")), root / i.toStdU16String());
        QCOMPARE(session->undeterminedNames({}), QStringList{QString::fromUtf8("新.wav")});
        QCOMPARE(session->logicalPathOf(u.toStdU16String()), fs::path(u"う"));
    }

    // Acceptance criterion 6 of docs/Editing.md at the level of a session: after one entry is
    // edited and saved, the encoding is unchanged and every other entry is written as it was.
    void saving_writes_the_edit_and_nothing_else() {
        const auto entry = directories().at(0).otoEntries().at(1);
        {
            auto transaction = m_session->transaction(QStringLiteral("Edit"));
            entry.setOffset(7);
            QVERIFY(transaction.commit());
        }
        DiagnosticList diagnostics;
        QVERIFY(m_session->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 QByteArray("a.wav=a,41.0,2,3,4,5\r\nb.wav=b,7,2,3,4,5\r\n"));
        QCOMPARE(read(QStringLiteral("sub/oto.ini")), QByteArray("x.wav=x,1,2,3,4,5\r\n"));
    }

    // The files that a save writes follow the undo history, without comparing contents. See the
    // section on the files a save writes in docs/Editing.md.
    void saving_without_edits_writes_nothing() {
        const auto before = times();
        save();
        QCOMPARE(writtenSince(before), QStringList());
    }

    void undoing_to_the_saved_step_writes_nothing() {
        editFirstEntry({}, 7);
        save();
        editFirstEntry({}, 8);
        m_session->undo();
        const auto before = times();
        save();
        QCOMPARE(writtenSince(before), QStringList());
    }

    void an_edit_undone_before_another_is_not_written() {
        editFirstEntry({}, 7);
        m_session->undo();
        editFirstEntry("sub", 7);
        const auto before = times();
        save();
        QCOMPARE(writtenSince(before), QStringList{QStringLiteral("sub/oto.ini")});
    }

    // The saved step left the history, so the file written at it is written again with the
    // contents of the current step.
    void a_truncated_history_writes_the_files_of_the_discarded_steps() {
        editFirstEntry({}, 7);
        save();
        m_session->undo();
        editFirstEntry("sub", 7);
        const auto before = times();
        save();
        QCOMPARE(writtenSince(before),
                 (QStringList{QStringLiteral("oto.ini"), QStringLiteral("sub/oto.ini")}));
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 QByteArray("a.wav=a,41.0,2,3,4,5\r\nb.wav=b,1,2,3,4,5\r\n"));
    }

    // A reload makes the directory hold the content on disk, and undoing it writes the former
    // contents back.
    void undoing_a_reload_writes_the_directory_back() {
        write(QStringLiteral("sub/oto.ini"), "x.wav=theirs,1,2,3,4,5\r\n");
        QCOMPARE(reload().changed, QList<fs::path>{fs::path("sub")});
        auto before = times();
        save();
        QCOMPARE(writtenSince(before), QStringList());

        m_session->undo();
        before = times();
        save();
        QCOMPARE(writtenSince(before), QStringList{QStringLiteral("sub/oto.ini")});
        QVERIFY(read(QStringLiteral("sub/oto.ini")).contains("x.wav=x,"));
    }

    // The files of the root belong to the voice bank as a whole, and each is written by its own
    // edits: a value of the root, a record replaced in the root, and a value in that record.
    void the_files_of_the_root_are_written_by_their_edits() {
        const VoiceBankRef bank(m_session.get());
        const auto edited = [this](const std::function<void()> &edit) -> QStringList {
            auto transaction = m_session->transaction(QStringLiteral("Edit"));
            edit();
            if (!transaction.commit()) {
                return {QStringLiteral("<not committed>")};
            }
            const auto before = times();
            save();
            return writtenSince(before);
        };
        QCOMPARE(edited([&bank] { bank.setReadme(QStringLiteral("about")); }),
                 QStringList{QStringLiteral("readme.txt")});
        VoiceCharacter character;
        character.name = QStringLiteral("n");
        QCOMPARE(edited([&] { bank.setCharacter(character); }),
                 QStringList{QStringLiteral("character.txt")});
        QCOMPARE(edited([&bank] { bank.character().setName(QStringLiteral("m")); }),
                 QStringList{QStringLiteral("character.txt")});
        QMap<int, VoicePrefix> map;
        map.insert(60, VoicePrefix{QString(), QStringLiteral("_B")});
        QCOMPARE(edited([&] { bank.setPrefixMap(map); }),
                 QStringList{QStringLiteral("prefix.map")});
        QCOMPARE(edited([&bank] {
                     bank.prefixMap().setValue(62, VoicePrefix{QString(), QStringLiteral("_C")});
                 }),
                 QStringList{QStringLiteral("prefix.map")});
    }

    void converting_writes_the_files_of_the_directory_and_its_configuration() {
        DiagnosticList diagnostics;
        QVERIFY(VoiceBankEdits::convertCharset(directories().at(indexOf({})),
                                               QStringLiteral("UTF-8"), diagnostics));
        const auto before = times();
        save();
        QCOMPARE(writtenSince(before),
                 (QStringList{QStringLiteral("hello-config.json"), QStringLiteral("oto.ini")}));
    }

    // The names of the audio files follow the record at once, and the save writes the record
    // into the configuration of its directory alone.
    void a_file_name_encoding_names_the_files_and_writes_the_configuration_only() {
        const auto misdecoded = QString::fromUtf8("偁.wav");
        write(misdecoded, "RIFF");
        DiagnosticList diagnostics;
        m_session->reloadFromDisk(m_session->checkDisk(), nullptr, diagnostics);
        QVERIFY(m_session->audioFiles({}).contains(misdecoded));

        VoiceBankConfiguration::FileNameEncoding record;
        record.original = QStringLiteral("Shift_JIS");
        record.misdecodedAs = QStringLiteral("GBK");
        record.files = VoiceBankConfiguration::FileNameEncoding::Files{{misdecoded}, {}};
        QVERIFY(VoiceBankEdits::setFileNameEncoding(directories().at(indexOf({})), record,
                                                    diagnostics));
        QVERIFY(m_session->audioFiles({}).contains(QString::fromUtf8("あ.wav")));
        QCOMPARE(m_session->audioPathOf({}, QString::fromUtf8("あ.wav")),
                 fs::path(pathOf(misdecoded).toStdU16String()));

        const auto before = times();
        save();
        QCOMPARE(writtenSince(before), QStringList{QStringLiteral("hello-config.json")});
        DiagnosticList ignored;
        const auto config = VoiceBankConfiguration::open(
            fs::path(pathOf(QStringLiteral("hello-config.json")).toStdU16String()), ignored);
        QVERIFY(config.has_value());
        QVERIFY(config->fileNameEncoding.has_value());
        QCOMPARE(config->fileNameEncoding, std::optional(record));
    }

    void a_remembered_encoding_writes_the_configuration_only() {
        m_session->rememberCharset("sub");
        QVERIFY(m_session->hasUnrecordedCharsets());
        const auto before = times();
        save();
        QCOMPARE(writtenSince(before), QStringList{QStringLiteral("sub/hello-config.json")});
        QVERIFY(!m_session->hasUnrecordedCharsets());
    }

    // An encoding selected by reading a directory again is recorded by the next save, although
    // no content changed, so the voice bank is unsaved until then.
    void an_encoding_read_again_is_unrecorded_until_saved() {
        QVERIFY(!m_session->hasUnrecordedCharsets());
        DiagnosticList diagnostics;
        QVERIFY(m_session->reread(fs::path("sub"), QStringLiteral("GBK"), diagnostics));
        QVERIFY(m_session->hasUnrecordedCharsets());
        QVERIFY(m_session->save(diagnostics));
        QVERIFY(!m_session->hasUnrecordedCharsets());
        QVERIFY(read(QStringLiteral("sub/hello-config.json")).contains("GBK"));
    }

    // A configuration changed elsewhere is not reported, and applying the check reads nothing
    // and creates no undo step.
    void a_configuration_changed_elsewhere_changes_nothing_in_the_tree() {
        write(QStringLiteral("sub/hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"GBK"})");
        const auto found = m_session->checkDisk();
        QVERIFY(found.isEmpty());
        DiagnosticList diagnostics;
        m_session->reloadFromDisk(found, nullptr, diagnostics);
        QCOMPARE(m_session->currentStep(), 0);
        QCOMPARE(directories().at(indexOf("sub")).charset(), QStringLiteral("UTF-8"));
    }

    // A file changed on disk is read again into the tree as one undo step. Undoing it restores
    // the edited version, which saving writes over the file, as the user chose it.
    void a_changed_directory_is_read_again_as_one_step() {
        write(QStringLiteral("sub/oto.ini"), "x.wav=y,1,2,3,4,5\r\n");
        const auto done = reload();
        QCOMPARE(done.changed, QList<fs::path>{fs::path("sub")});
        const auto sub = directories().at(indexOf("sub"));
        QCOMPARE(sub.otoEntries().at(0).alias(), QStringLiteral("y"));
        QCOMPARE(m_session->currentStep(), 1);
        QCOMPARE(m_session->undoMessage(), QStringLiteral("Reload from Disk"));

        m_session->undo();
        QCOMPARE(directories().at(indexOf("sub")).otoEntries().at(0).alias(), QStringLiteral("x"));
        DiagnosticList diagnostics;
        QVERIFY(m_session->save(diagnostics));
        QVERIFY(read(QStringLiteral("sub/oto.ini")).contains("x.wav=x,"));
    }

    // A directory removed on disk leaves the tree. Undoing the reload restores it, and saving
    // creates it again.
    void a_removed_directory_returns_on_undo_and_is_created_by_saving() {
        QVERIFY(QDir(pathOf(QStringLiteral("sub"))).removeRecursively());
        QCOMPARE(reload().removed, QList<fs::path>{fs::path("sub")});
        QCOMPARE(indexOf("sub"), -1);

        m_session->undo();
        QVERIFY(indexOf("sub") > 0);
        DiagnosticList diagnostics;
        QVERIFY(m_session->save(diagnostics));
        QVERIFY(read(QStringLiteral("sub/oto.ini")).contains("x.wav=x,1,2,3,4,5"));
    }

    // A directory added on disk enters the tree. Undoing the reload takes it out again, and
    // saving leaves it on disk as it is.
    void an_added_directory_leaves_the_tree_on_undo_and_stays_on_disk() {
        write(QStringLiteral("new/oto.ini"), "n.wav=n,1,2,3,4,5\r\n");
        DirectorySelector selector({
            {fs::path("new"), QStringLiteral("UTF-8")},
        });
        DiagnosticList diagnostics;
        const auto done = m_session->reloadFromDisk(m_session->checkDisk(), &selector, diagnostics);
        QCOMPARE(done.added, QList<fs::path>{fs::path("new")});
        QVERIFY(indexOf("new") > 0);

        m_session->undo();
        QCOMPARE(indexOf("new"), -1);
        QVERIFY(m_session->save(diagnostics));
        QCOMPARE(read(QStringLiteral("new/oto.ini")), QByteArray("n.wav=n,1,2,3,4,5\r\n"));

        // Every later check reports it as new, so that it can be taken in again.
        const auto again = m_session->checkDisk();
        QCOMPARE(again.added, QList<fs::path>{fs::path("new")});
        QCOMPARE(m_session->checkDisk({fs::path(m_dir->path().toStdU16String())}).added,
                 QList<fs::path>{fs::path("new")});
        m_session->reloadFromDisk(again, &selector, diagnostics);
        QVERIFY(indexOf("new") > 0);
        QVERIFY(m_session->checkDisk().isEmpty());

        // Removed on disk after another undo, it is reported as removed and not as new.
        m_session->undo();
        QVERIFY(QDir(pathOf(QStringLiteral("new"))).removeRecursively());
        const auto gone = m_session->checkDisk();
        QCOMPARE(gone.removed, QList<fs::path>{fs::path("new")});
        QVERIFY(gone.added.isEmpty());
    }

    // A new directory that a reload of the audio files of its parent took into the file system
    // state, but not into the tree, is read by reading everything.
    void reading_everything_takes_a_directory_left_untaken() {
        write(QStringLiteral("new/oto.ini"), "n.wav=n,1,2,3,4,5\r\n");
        write(QStringLiteral("c.wav"), "RIFF");
        DirectorySelector selector({
            {fs::path("new"), QStringLiteral("UTF-8")},
        });
        DiagnosticList diagnostics;
        const auto found = m_session->checkDisk();
        QCOMPARE(found.added, QList<fs::path>{fs::path("new")});
        VoiceBankChanges audio;
        audio.audio = found.audio;
        m_session->reloadFromDisk(audio, &selector, diagnostics);
        QCOMPARE(indexOf("new"), -1);
        QCOMPARE(m_session->checkDisk().added, QList<fs::path>{fs::path("new")});

        const auto done = m_session->reloadAllFromDisk(&selector, diagnostics);
        QVERIFY(done.added.contains(fs::path("new")));
        QVERIFY(indexOf("new") > 0);
        QVERIFY(m_session->checkDisk().isEmpty());

        // Nor does such a reload hide a subdirectory removed since.
        write(QStringLiteral("d.wav"), "RIFF");
        VoiceBankChanges more;
        more.audio = m_session->checkDisk().audio;
        QVERIFY(QDir(pathOf(QStringLiteral("sub"))).removeRecursively());
        m_session->reloadFromDisk(more, &selector, diagnostics);
        QCOMPARE(m_session->checkDisk().removed, QList<fs::path>{fs::path("sub")});
    }

    // A change of the audio files alone updates the samples without an entry, and is not an
    // undo step.
    void a_new_audio_file_is_no_undo_step() {
        write(QStringLiteral("c.wav"), "RIFF");
        QCOMPARE(reload().audio, QList<fs::path>{fs::path()});
        QCOMPARE(m_session->currentStep(), 0);
        const auto bank = m_session->snapshot();
        QVERIFY(bank.find(60, QStringLiteral("c")));
    }

    // The files on disk are taken as they are, although they violate a constraint.
    void a_reload_takes_what_the_file_holds() {
        write(QStringLiteral("sub/oto.ini"), "x.wav=x,1,2,3,4,5\r\nx.wav=x,1,2,3,4,5\r\n");
        QCOMPARE(reload().changed, QList<fs::path>{fs::path("sub")});
        QCOMPARE(directories().at(indexOf("sub")).otoEntries().size(), 2);
        QCOMPARE(m_session->currentStep(), 1);
    }

    // Reading everything again finds a change that the stamp misses: the same size and time.
    void reloading_everything_reads_what_a_stamp_misses() {
        const auto time = QDateTime(QDate(2020, 1, 1), QTime(0, 0));
        const auto setTime = [this, &time] {
            QFile file(pathOf(QStringLiteral("sub/oto.ini")));
            QVERIFY(file.open(QIODevice::ReadWrite));
            QVERIFY(file.setFileTime(time, QFileDevice::FileModificationTime));
        };
        setTime();
        reload();
        QVERIFY(m_session->checkDisk().isEmpty());
        write(QStringLiteral("sub/oto.ini"), "x.wav=y,1,2,3,4,5\r\n");
        setTime();
        QVERIFY(m_session->checkDisk().isEmpty());

        const auto step = m_session->currentStep();
        DiagnosticList diagnostics;
        m_session->reloadAllFromDisk(nullptr, diagnostics);
        QCOMPARE(directories().at(indexOf("sub")).otoEntries().at(0).alias(), QStringLiteral("y"));
        QCOMPARE(m_session->currentStep(), step + 1);
    }

    // Without its root, the voice bank on disk is incomplete, and the tree is the only copy.
    // Saving writes every text file of it again, which makes the voice bank complete.
    void a_removed_root_is_written_again_by_saving() {
        QVERIFY(!m_session->isIncomplete());
        QVERIFY(QDir(m_dir->path()).removeRecursively());
        QVERIFY(m_session->checkDisk().rootNotFound);
        QVERIFY(m_session->isIncomplete());
        QCOMPARE(directories().size(), 2);

        DiagnosticList diagnostics;
        QVERIFY(m_session->save(diagnostics));
        QVERIFY(!m_session->isIncomplete());
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 QByteArray("a.wav=a,41.0,2,3,4,5\r\nb.wav=b,1,2,3,4,5\r\n"));
        QCOMPARE(read(QStringLiteral("sub/oto.ini")),
                 QByteArray("#Charset:UTF-8\r\nx.wav=x,1,2,3,4,5\r\n"));
        QVERIFY(!QFileInfo::exists(pathOf(QStringLiteral("left"))));
        QVERIFY(m_session->checkDisk().isEmpty());
    }

    // A root restored on disk by another program makes the voice bank complete again.
    void a_restored_root_makes_the_voice_bank_complete() {
        const auto moved = m_dir->path() + QStringLiteral("-moved");
        QVERIFY(QDir().rename(m_dir->path(), moved));
        QVERIFY(m_session->checkDisk().rootNotFound);
        QVERIFY(m_session->isIncomplete());
        QVERIFY(QDir().rename(moved, m_dir->path()));
        QVERIFY(!m_session->checkDisk().rootNotFound);
        QVERIFY(!m_session->isIncomplete());
    }

    // A root without text that gains a text file whose encoding nobody selects is not read
    // again. The tree keeps the content read before, and the voice bank on disk is incomplete
    // until the root is read in an encoding.
    void a_root_not_read_again_makes_the_voice_bank_incomplete() {
        QTemporaryDir dir;
        QVERIFY(writeSampleFile(dir.path(), QStringLiteral("a.wav"), "RIFF"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(fs::path(dir.path().toStdU16String()), nullptr,
                                                     diagnostics);
        QVERIFY(opened);
        const auto session = VoiceBankSession::create(std::move(*opened), diagnostics);
        QVERIFY(session);

        QVERIFY(writeSampleFile(dir.path(), QStringLiteral("readme.txt"), "new"));
        const auto done = session->reloadFromDisk(session->checkDisk(), nullptr, diagnostics);
        QCOMPARE(done.changed, QList<fs::path>{fs::path()});
        QVERIFY(session->isIncomplete());
        QVERIFY(VoiceBankRef(session.get()).readme().isEmpty());

        QVERIFY(session->reread({}, QStringLiteral("GBK"), diagnostics));
        QVERIFY(!session->isIncomplete());
        QCOMPARE(VoiceBankRef(session.get()).readme(), QStringLiteral("new"));
    }

    // A text file that appears in a directory already read is read in its encoding, without
    // asking again.
    void a_text_file_that_appears_is_read_in_the_encoding_of_its_directory() {
        write(QStringLiteral("readme.txt"), kGbkGePing);
        QCOMPARE(reload().changed, QList<fs::path>{fs::path()});
        QVERIFY(!m_session->isIncomplete());
        QCOMPARE(VoiceBankRef(m_session.get()).readme(),
                 QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3"));
    }

    // Where the root cannot be written again, the voice bank is saved elsewhere and edited there
    // from now on. Every other file is copied by default, including the directories that are not
    // in the tree, which remain excluded.
    void saving_as_moves_the_session_to_the_new_folder() {
        write(QStringLiteral("readme.pdf"), "%PDF");
        QTemporaryDir target;
        const auto folder = fs::path(target.path().toStdU16String()) / "copy";
        const auto original = read(QStringLiteral("oto.ini"));

        DiagnosticList diagnostics;
        QVERIFY(m_session->saveAs(folder, VoiceBankSession::AllFiles, diagnostics));
        QCOMPARE(m_session->rootPath(), folder);
        const auto copied = [&folder](const char *relative) {
            QFile file(QString::fromStdU16String((folder / relative).u16string()));
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
        };
        QCOMPARE(copied("a.wav"), QByteArray("RIFF"));
        QCOMPARE(copied("readme.pdf"), QByteArray("%PDF"));
        QCOMPARE(copied("left/oto.ini"), "z.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        QCOMPARE(m_session->excludedDirectories().size(), 1);
        QVERIFY(m_session->snapshot().find(60, QStringLiteral("a")));

        // An edit is saved into the new folder, and the original stays as it was.
        {
            auto transaction = m_session->transaction(QStringLiteral("Edit"));
            directories().at(0).otoEntries().at(1).setOffset(7);
            QVERIFY(transaction.commit());
        }
        QVERIFY(m_session->save(diagnostics));
        QVERIFY(copied("oto.ini").contains("b.wav=b,7,"));
        QCOMPARE(read(QStringLiteral("oto.ini")), original);
    }

    // Saving only the text files leaves the audio files behind, and a folder that is not empty
    // is refused.
    void saving_as_text_files_leaves_the_rest_behind() {
        QTemporaryDir target;
        const auto folder = fs::path(target.path().toStdU16String());
        DiagnosticList diagnostics;
        QVERIFY(m_session->saveAs(folder, VoiceBankSession::TextFiles, diagnostics));
        QVERIFY(QFileInfo::exists(target.filePath(QStringLiteral("oto.ini"))));
        QVERIFY(!QFileInfo::exists(target.filePath(QStringLiteral("a.wav"))));
        QVERIFY(!QFileInfo::exists(target.filePath(QStringLiteral("left"))));
        QVERIFY(m_session->excludedDirectories().isEmpty());

        diagnostics.clear();
        QVERIFY(!m_session->saveAs(folder, VoiceBankSession::TextFiles, diagnostics));
        QVERIFY(hasError(diagnostics));
        QCOMPARE(m_session->rootPath(), folder);
    }

    // The files of the root read again replace the character, the prefix map and the readme.
    void the_files_of_the_root_are_read_again() {
        write(QStringLiteral("character.txt"), "name=n\r\n");
        QCOMPARE(reload().changed, QList<fs::path>{fs::path()});
        QCOMPARE(VoiceBankRef(m_session.get()).character().name(), QStringLiteral("n"));
        m_session->undo();
        QVERIFY(!VoiceBankRef(m_session.get()).character().isValid());
    }

    // A directory that was left out enters the tree once read in an encoding, and leaves the
    // excluded directories.
    void an_excluded_directory_is_read_in_an_encoding() {
        QCOMPARE(m_session->excludedDirectories().size(), 1);
        QCOMPARE(indexOf("left"), -1);
        DiagnosticList diagnostics;
        QVERIFY(m_session->reread("left", QStringLiteral("GBK"), diagnostics));
        QVERIFY(indexOf("left") > 0);
        QVERIFY(m_session->excludedDirectories().isEmpty());
        QCOMPARE(directories().at(indexOf("left")).otoEntries().at(0).alias(),
                 QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3"));
        QCOMPARE(m_session->undoMessage(), QStringLiteral("Read Again in GBK"));

        m_session->undo();
        QCOMPARE(indexOf("left"), -1);
    }

    // Several directories read again in their encodings form one undo step.
    void several_directories_are_read_again_as_one_step() {
        write(QStringLiteral("sub/oto.ini"), "x.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        QCOMPARE(reload().changed, QList<fs::path>{fs::path("sub")});
        const auto step = m_session->currentStep();
        const auto alias = [this](const fs::path &directory) {
            return directories().at(indexOf(directory)).otoEntries().at(0).alias();
        };
        const auto before = alias("sub");

        DiagnosticList diagnostics;
        QVERIFY(m_session->reread(
            {
                {fs::path(),      QStringLiteral("Shift_JIS")},
                {fs::path("sub"), QStringLiteral("GBK")      },
        },
            diagnostics));
        QCOMPARE(m_session->currentStep(), step + 1);
        QCOMPARE(m_session->undoMessage(), QStringLiteral("Read Again in Encodings"));
        QCOMPARE(directories().at(0).charset(), QStringLiteral("Shift_JIS"));
        QCOMPARE(alias("sub"), QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3"));

        m_session->undo();
        QCOMPARE(directories().at(0).charset(), QStringLiteral("GBK"));
        QCOMPARE(alias("sub"), before);
    }

    // A directory that cannot be read again is reported, and the others are read regardless.
    void a_directory_not_read_again_leaves_the_others_read() {
        DiagnosticList diagnostics;
        QVERIFY(!m_session->reread(
            {
                {fs::path("none"), QStringLiteral("GBK")},
                {fs::path("sub"),  QStringLiteral("GBK")},
        },
            diagnostics));
        QVERIFY(hasError(diagnostics));
        QCOMPARE(directories().at(indexOf("sub")).charset(), QStringLiteral("GBK"));
        QCOMPARE(m_session->currentStep(), 1);
    }

    // A directory read again in an encoding in which some bytes are invalid stays in the tree,
    // with U+FFFD in their place. Undoing it restores the text read before.
    void a_directory_read_in_a_wrong_encoding_stays_in_the_tree() {
        write(QStringLiteral("sub/oto.ini"), "x.wav=\xe8\x91\x9b,1,2,3,4,5\r\n");
        QCOMPARE(reload().changed, QList<fs::path>{fs::path("sub")});
        DiagnosticList diagnostics;
        QVERIFY(m_session->reread("sub", QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(indexOf("sub") > 0);
        QCOMPARE(m_session->excludedDirectories().size(), 1);
        const auto alias = [this] {
            return directories().at(indexOf("sub")).otoEntries().at(0).alias();
        };
        QVERIFY(alias().contains(QChar::ReplacementCharacter));

        m_session->undo();
        QCOMPARE(alias(), QString::fromUtf8("\xe8\x91\x9b"));
    }

private:
    /// A Japanese voice bank extracted on a Chinese system in the fixture of open(): 偁.wav (あ)
    /// with its frq and 偄.wav (い) in the root, and the folder 偆 (う).
    void openMisdecoded() {
        write(QString::fromUtf8("偁.wav"), "RIFF");
        write(QString::fromUtf8("偁_wav.frq"), "FREQ0003");
        write(QString::fromUtf8("偄.wav"), "RIFF");
        write(QString::fromUtf8("偆/c.wav"), "RIFF");
        open();
    }

    static VoiceBankConfiguration::FileNameEncoding japaneseOnChinese() {
        VoiceBankConfiguration::FileNameEncoding record;
        record.original = QStringLiteral("Shift_JIS");
        record.misdecodedAs = QStringLiteral("GBK");
        return record;
    }

    std::optional<VoiceBankConfiguration::FileNameEncoding> recordOnDisk() const {
        DiagnosticList ignored;
        const auto config = VoiceBankConfiguration::open(
            fs::path(pathOf(QStringLiteral("hello-config.json")).toStdU16String()), ignored);
        return config ? config->fileNameEncoding : std::nullopt;
    }

private Q_SLOTS:
    // Every misdecoded name is renamed with its frq, and the record of the root has no
    // misdecoded name left. The encoding of the root is kept.
    void the_misdecoded_names_are_renamed_on_disk() {
        openMisdecoded();
        FrequencyFormats formats;
        const BuiltinFrequencyFormats builtins(formats.registry());
        DiagnosticList diagnostics;
        QVERIFY(m_session->renameOnDisk(
            {
                {{},
                 japaneseOnChinese(),
                 {QString::fromUtf8("偁.wav"), QString::fromUtf8("偄.wav"),
                  QString::fromUtf8("偆")}}
        },
            &formats, diagnostics));
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(read(QString::fromUtf8("あ.wav")), QByteArray("RIFF"));
        QCOMPARE(read(QString::fromUtf8("あ_wav.frq")), QByteArray("FREQ0003"));
        QCOMPARE(read(QString::fromUtf8("い.wav")), QByteArray("RIFF"));
        QCOMPARE(read(QString::fromUtf8("う/c.wav")), QByteArray("RIFF"));
        QCOMPARE(read(QString::fromUtf8("偁.wav")), QByteArray("<missing>"));
        QCOMPARE(recordOnDisk(), std::optional(VoiceBankConfiguration::FileNameEncoding()));
        DiagnosticList ignored;
        const auto config = VoiceBankConfiguration::open(
            fs::path(pathOf(QStringLiteral("hello-config.json")).toStdU16String()), ignored);
        QCOMPARE(config->charset, QStringLiteral("GBK"));
    }

    // A misdecoded name left keeps the record with lists, the renamed name in the correct list.
    void a_part_of_the_names_is_renamed() {
        openMisdecoded();
        DiagnosticList diagnostics;
        QVERIFY(m_session->renameOnDisk(
            {
                {{}, japaneseOnChinese(), {QString::fromUtf8("偁.wav")}}
        },
            nullptr, diagnostics));
        const auto record = recordOnDisk();
        QVERIFY(record && record->files);
        QCOMPARE(record->files->misdecoded,
                 (QStringList{QString::fromUtf8("偄.wav"), QString::fromUtf8("偆")}));
        QCOMPARE(record->files->correct, QStringList{QString::fromUtf8("あ.wav")});
        QCOMPARE(read(QString::fromUtf8("偁_wav.frq")), QByteArray("FREQ0003"));
    }

    // The maps of a format that cannot follow the renames are left, which is reported once, and
    // the names are renamed nevertheless. An audio file without a map of the format is not
    // reported.
    void a_format_without_renames_is_reported() {
        class Unsupported : public FrequencyFormat {
        public:
            QString id() const override {
                return QStringLiteral("u");
            }
            QString name() const override {
                return QStringLiteral("u");
            }
            QStringList resamplerPatterns() const override {
                return {};
            }
            QList<bool> mapsExist(const fs::path &, const QList<fs::path> &wavs) const override {
                QList<bool> found;
                for (const auto &wav : wavs) {
                    found.push_back(wav.filename() == fs::path(u"偁.wav"));
                }
                return found;
            }
            std::optional<FrequencyMap> read(const fs::path &, int,
                                             DiagnosticList &) const override {
                return std::nullopt;
            }
        };
        openMisdecoded();
        FrequencyFormats formats;
        const FrequencyFormatRegistry::Add<Unsupported> unsupported(formats.registry(), "u", {});
        DiagnosticList diagnostics;
        QVERIFY(m_session->renameOnDisk(
            {
                {{},
                 japaneseOnChinese(),
                 {QString::fromUtf8("偁.wav"), QString::fromUtf8("偄.wav")}}
        },
            &formats, diagnostics));
        QCOMPARE(read(QString::fromUtf8("あ.wav")), QByteArray("RIFF"));
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);
    }

    // 你 in GBK is an invalid byte sequence in Shift_JIS, so it has no restored name.
    void a_name_that_does_not_restore_renames_nothing() {
        write(QString::fromUtf8("你.wav"), "RIFF");
        openMisdecoded();
        DiagnosticList diagnostics;
        QVERIFY(!m_session->renameOnDisk(
            {
                {{},
                 japaneseOnChinese(),
                 {QString::fromUtf8("偁.wav"), QString::fromUtf8("你.wav")}}
        },
            nullptr, diagnostics));
        QVERIFY(hasError(diagnostics));
        QCOMPARE(read(QString::fromUtf8("偁.wav")), QByteArray("RIFF"));
        QCOMPARE(read(QString::fromUtf8("あ.wav")), QByteArray("<missing>"));
    }

    // Each directory is listed with its names on disk and the state of the tree, in the order
    // of the paths. The excluded directory has no entries.
    void the_names_of_every_directory_are_listed() {
        openMisdecoded();
        const auto record = japaneseOnChinese();
        DiagnosticList diagnostics;
        QVERIFY(VoiceBankEdits::setFileNameEncoding(directories().at(0), record, diagnostics));

        const auto listings = m_session->nameListings();
        QList<fs::path> paths;
        for (const auto &listing : listings) {
            paths.push_back(listing.directory);
        }
        QCOMPARE(paths,
                 (QList<fs::path>{fs::path(), fs::path("left"), fs::path("sub"), fs::path(u"偆")}));
        const auto &root = listings.at(0);
        QCOMPARE(root.charset, QStringLiteral("GBK"));
        QCOMPARE(root.record, std::optional(record));
        QCOMPARE(root.entries, (QStringList{QStringLiteral("a.wav"), QStringLiteral("b.wav")}));
        auto audio = root.audioFiles;
        audio.sort();
        QCOMPARE(audio, (QStringList{QStringLiteral("a.wav"), QString::fromUtf8("偁.wav"),
                                     QString::fromUtf8("偄.wav")}));
        auto subdirectories = root.subdirectories;
        subdirectories.sort();
        QCOMPARE(subdirectories, (QStringList{QStringLiteral("left"), QStringLiteral("sub"),
                                              QString::fromUtf8("偆")}));
        QVERIFY(listings.at(1).entries.isEmpty());
        QCOMPARE(listings.at(2).entries, QStringList{QStringLiteral("x.wav")});
        QCOMPARE(listings.at(3).audioFiles, QStringList{QStringLiteral("c.wav")});
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankSession_Disk)

#include "test_VoiceBankSession_Disk.moc"
