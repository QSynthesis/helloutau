#include <memory>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Edit/VoiceBankEdits.h>
#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/Edit/VoiceBankSession.h>

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
        auto opened = VoiceBankDiskState::open(fs::path(m_dir->path().toStdU16String()), &selector,
                                               diagnostics);
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

    // A configuration changed elsewhere is reported for the editor to mark the voice bank unsaved,
    // and applying the check reads nothing and creates no undo step.
    void a_configuration_changed_elsewhere_changes_nothing_in_the_tree() {
        write(QStringLiteral("sub/hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"GBK"})");
        const auto found = m_session->checkDisk();
        QCOMPARE(found.config, QList<fs::path>{fs::path("sub")});
        QVERIFY(found.changed.isEmpty());
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

    // A root that no longer reads when read again keeps its former contents in the tree, and
    // saving writes them over the files.
    void a_root_that_no_longer_reads_is_written_back() {
        write(QStringLiteral("oto.ini"), "a.wav=\xff,41.0,2,3,4,5\r\n");
        DiagnosticList diagnostics;
        const auto done = m_session->reloadFromDisk(m_session->checkDisk(), nullptr, diagnostics);
        QCOMPARE(done.changed, QList<fs::path>{fs::path()});
        QVERIFY(m_session->isIncomplete());
        QCOMPARE(diagnostics.first().severity, DiagnosticSeverity::Warning);
        QCOMPARE(directories().at(0).otoEntries().size(), 2);
        QCOMPARE(directories().at(0).otoEntries().at(0).alias(), QStringLiteral("a"));

        QVERIFY(m_session->save(diagnostics));
        QVERIFY(!m_session->isIncomplete());
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 QByteArray("a.wav=a,41.0,2,3,4,5\r\nb.wav=b,1,2,3,4,5\r\n"));
    }

    // A root that reads again once its files are repaired makes the voice bank complete.
    void a_root_read_again_makes_the_voice_bank_complete() {
        write(QStringLiteral("oto.ini"), "a.wav=\xff,41.0,2,3,4,5\r\n");
        reload();
        QVERIFY(m_session->isIncomplete());
        write(QStringLiteral("oto.ini"), "a.wav=c,41.0,2,3,4,5\r\n");
        QFile file(pathOf(QStringLiteral("oto.ini")));
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(-7200),
                                 QFileDevice::FileModificationTime));
        file.close();
        QCOMPARE(reload().changed, QList<fs::path>{fs::path()});
        QVERIFY(!m_session->isIncomplete());
        QCOMPARE(directories().at(0).otoEntries().at(0).alias(), QStringLiteral("c"));
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

    // A directory read again in an encoding in which it does not decode leaves the tree for the
    // excluded directories. Undoing it returns it to the tree.
    void a_directory_read_in_a_wrong_encoding_leaves_the_tree() {
        write(QStringLiteral("sub/oto.ini"), "x.wav=\xe8\x91\x9b,1,2,3,4,5\r\n");
        QCOMPARE(reload().changed, QList<fs::path>{fs::path("sub")});
        DiagnosticList diagnostics;
        QVERIFY(m_session->reread("sub", QStringLiteral("Shift_JIS"), diagnostics));
        QCOMPARE(indexOf("sub"), -1);
        QCOMPARE(m_session->excludedDirectories().size(), 2);

        m_session->undo();
        QVERIFY(indexOf("sub") > 0);
        QCOMPARE(m_session->excludedDirectories().size(), 1);
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankSession_Disk)

#include "test_VoiceBankSession_Disk.moc"
