#include <algorithm>
#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/VoiceBank.h>
#include <hellokit/VoiceBank/VoiceBankDiskState.h>

using namespace hello::kit;

// あ in Shift_JIS.
static const QByteArray kShiftJisA = QByteArray("\x82\xa0", 2);
static const QString kA = QString::fromUtf8("\xe3\x81\x82");

namespace fs = std::filesystem;

namespace {

    /// Selects one fixed encoding for every query and counts the queries.
    class CountingSelector : public VoiceBankCharsetSelector {
    public:
        explicit CountingSelector(QString charset) : m_charset(std::move(charset)) {
        }

        int asked = 0;

        std::optional<QString> selectCharset(const VoiceBankDirectorySource &,
                                             DiagnosticList &) override {
            ++asked;
            return m_charset;
        }

    private:
        QString m_charset;
    };

    /// The result for a user who accepts every reload.
    VoiceBankChanges takeIn(VoiceBank &bank, VoiceBankDiskState &disk,
                            VoiceBankCharsetSelector *selector,
                            const QList<fs::path> &places = {}) {
        DiagnosticList diagnostics;
        const auto found = places.isEmpty() ? disk.checkDisk() : disk.checkDisk(places);
        disk.reloadFromDisk(bank, found, selector, diagnostics);
        return found;
    }

}

class test_VoiceBankDiskState_Changes : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    fs::path root() const {
        return fs::path(m_dir->path().toStdU16String()) / "bank";
    }

    QString pathOf(const QString &relative) const {
        return m_dir->path() + QStringLiteral("/bank/") + relative;
    }

    void write(const QString &relative, const QByteArray &bytes) {
        const QString path = pathOf(relative);
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    std::optional<VoiceBankDiskState::Opened> open(VoiceBankCharsetSelector *selector) const {
        DiagnosticList diagnostics;
        return VoiceBankDiskState::open(root(), selector, diagnostics);
    }

    static const VoiceBankDirectory &directoryOf(const VoiceBank &bank, const VoiceSample &sample) {
        return bank.directories().at(sample.directory);
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        QVERIFY(QDir().mkpath(pathOf(QString())));
    }

    void cleanup() {
        m_dir.reset();
    }

    // Applying changes is the user's decision, so a check only reports them.
    void a_check_changes_nothing_in_the_bank() {
        write(QStringLiteral("oto.ini"), "a.wav=old,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        write(QStringLiteral("oto.ini"), "a.wav=newer,10,2,3,4,5\r\n");
        const auto found = disk.checkDisk();
        QCOMPARE(found.changed, QList<fs::path>{fs::path()});
        QVERIFY(bank.find(60, QStringLiteral("old")));
        QVERIFY(!bank.find(60, QStringLiteral("newer")));

        DiagnosticList diagnostics;
        disk.reloadFromDisk(bank, found, &selector, diagnostics);
        QVERIFY(!bank.find(60, QStringLiteral("old")));
        const auto *sample = bank.find(60, QStringLiteral("newer"));
        QVERIFY(sample);
        QCOMPARE(sample->offset, 10.0);
        QVERIFY(disk.checkDisk().isEmpty());
    }

    void a_sample_added_on_disk_is_found() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        write(QStringLiteral("ka.wav"), "RIFF");
        QCOMPARE(takeIn(bank, disk, &selector).audio, QList<fs::path>{fs::path()});
        QVERIFY(bank.find(60, QStringLiteral("ka")));
    }

    void a_sample_removed_on_disk_is_dropped() {
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("ka.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        QVERIFY(QFile::remove(pathOf(QStringLiteral("ka.wav"))));
        takeIn(bank, disk, &selector);
        QVERIFY(!bank.find(60, QStringLiteral("ka")));
        QVERIFY(bank.find(60, QStringLiteral("a")));
    }

    // A new audio file adds a sample without an entry and is applied without a decision of the
    // user, so it must not discard the unsaved changes of the directory as a reread would.
    void an_added_audio_file_keeps_what_was_not_saved() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);

        write(QStringLiteral("ka.wav"), "RIFF");
        const auto found = takeIn(bank, disk, &selector);
        QVERIFY(found.changed.isEmpty());
        QCOMPARE(found.audio, QList<fs::path>{fs::path()});
        const auto *edited = bank.find(60, QStringLiteral("a"));
        QVERIFY(edited);
        QCOMPARE(edited->offset, 7.0);
        QVERIFY(bank.find(60, QStringLiteral("ka")));
        QVERIFY(disk.checkDisk().isEmpty());

        // The file of the entry is not added again as a sample without an entry.
        const auto &all = bank.samples();
        QCOMPARE(int(std::count_if(all.begin(), all.end(),
                                   [](const VoiceSample &sample) {
                                       return sample.fileName == QStringLiteral("a.wav");
                                   })),
                 1);
    }

    // The entry of a removed audio file is kept for the user to decide on, while a removed file
    // without an entry leaves no sample behind.
    void a_removed_audio_file_keeps_its_entry() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("ki.wav"), "RIFF");
        write(QStringLiteral("ku.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        QVERIFY(QFile::remove(pathOf(QStringLiteral("a.wav"))));
        QVERIFY(QFile::remove(pathOf(QStringLiteral("ki.wav"))));
        QCOMPARE(takeIn(bank, disk, &selector).audio, QList<fs::path>{fs::path()});
        const auto *entry = bank.find(60, QStringLiteral("a"));
        QVERIFY(entry);
        QVERIFY(entry->hasEntry);
        QVERIFY(!bank.find(60, QStringLiteral("ki")));
        QVERIFY(bank.find(60, QStringLiteral("ku")));
    }

    // A text file changed after the check that found only new audio files is not taken as read
    // by applying that check, and the next check reports it.
    void a_text_change_after_an_audio_check_is_still_reported() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        write(QStringLiteral("ka.wav"), "RIFF");
        const auto found = disk.checkDisk();
        QCOMPARE(found.audio, QList<fs::path>{fs::path()});
        write(QStringLiteral("oto.ini"), "a.wav=changed,1,2,3,4,5\r\n");

        // Dated back, so that the stamp alone decides and no comparison by content hides a
        // stamp taken wrongly.
        QFile oto(pathOf(QStringLiteral("oto.ini")));
        QVERIFY(oto.open(QIODevice::ReadWrite));
        QVERIFY(oto.setFileTime(QDateTime::currentDateTime().addSecs(-3600),
                                QFileDevice::FileModificationTime));
        oto.close();

        DiagnosticList diagnostics;
        disk.reloadFromDisk(bank, found, &selector, diagnostics);
        QVERIFY(bank.find(60, QStringLiteral("ka")));
        QCOMPARE(disk.checkDisk().changed, QList<fs::path>{fs::path()});
    }

    // A resampler writes its analysis file beside the sample during rendering. This is not an
    // edit, and rendering must not make the voice bank appear modified.
    void what_a_render_leaves_beside_a_sample_changes_nothing() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &disk = opened->disk;

        write(QStringLiteral("a_wav.frq"), "FREQ");
        write(QStringLiteral("a.llsm"), "LLSM");
        QVERIFY(disk.checkDisk().isEmpty());
    }

    // The character.txt, prefix.map and readme.txt of a subdirectory are not part of this voice
    // bank, so changing them is not a change of it. The same files in the root are.
    void the_text_files_of_a_subdirectory_are_not_watched() {
        write(QStringLiteral("inner/oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("inner/a.wav"), "RIFF");
        write(QStringLiteral("inner/character.txt"), "name=inner\n");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &disk = opened->disk;

        write(QStringLiteral("inner/character.txt"), "name=changed inner\n");
        write(QStringLiteral("inner/prefix.map"), "C4\t\t_B\n");
        write(QStringLiteral("inner/readme.txt"), "readme");
        QVERIFY(disk.checkDisk().isEmpty());

        write(QStringLiteral("character.txt"), "name=root\n");
        QCOMPARE(disk.checkDisk().changed, QList<fs::path>{fs::path()});
    }

    // Two writes within one timestamp interval of the file system are indistinguishable by size
    // and time. Such a file is compared by content, because otherwise the second write would
    // never be detected.
    void a_change_inside_one_tick_of_the_clock_is_still_seen() {
        write(QStringLiteral("oto.ini"), "a.wav=aaa,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        const auto oto = root() / "oto.ini";
        const auto time = fs::last_write_time(oto);
        write(QStringLiteral("oto.ini"), "a.wav=bbb,1,2,3,4,5\r\n");
        fs::last_write_time(oto, time);

        QCOMPARE(takeIn(bank, disk, &selector).changed, QList<fs::path>{fs::path()});
        QVERIFY(bank.find(60, QStringLiteral("bbb")));
    }

    // A detected but unapplied change is reported again, so that a check whose result was
    // missed loses nothing. The files of the root remain unchanged here, so that only its
    // listing reveals the added and removed entries, and the stamp of the root must not advance.
    void what_is_not_taken_in_is_found_again() {
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("kept/oto.ini"), "k.wav=k,1,2,3,4,5\r\n");
        write(QStringLiteral("kept/k.wav"), "RIFF");
        write(QStringLiteral("going/b.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        write(QStringLiteral("kept/oto.ini"), "k.wav=changed,1,2,3,4,5\r\n");
        write(QStringLiteral("coming/c.wav"), "RIFF");
        QVERIFY(QDir(pathOf(QStringLiteral("going"))).removeRecursively());

        for (int i = 0; i < 3; ++i) {
            const auto found = disk.checkDisk();
            QCOMPARE(found.changed, QList<fs::path>{fs::path("kept")});
            QCOMPARE(found.added, QList<fs::path>{fs::path("coming")});
            QCOMPARE(found.removed, QList<fs::path>{fs::path("going")});
        }
        QVERIFY(bank.find(60, QStringLiteral("k")));
    }

    // Changed on disk and modified in memory: two versions, between which the user must
    // choose. Nothing here decides on the user's behalf, and a reload represents choosing the
    // version on disk.
    void a_change_on_disk_leaves_one_not_saved_alone_until_the_user_chooses() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        auto samples = bank.samples();
        samples[0].offset = 99;
        bank.setSamples(samples);

        write(QStringLiteral("oto.ini"), "a.wav=theirs,7,2,3,4,5\r\n");
        const auto found = disk.checkDisk();
        QCOMPARE(found.changed, QList<fs::path>{fs::path()});
        QVERIFY(disk.isModified(bank, bank.directories().at(0).path));
        QCOMPARE(bank.samples().at(0).offset, 99.0);

        DiagnosticList diagnostics;
        disk.reloadFromDisk(bank, found, &selector, diagnostics);
        QVERIFY(!disk.isModified(bank, bank.directories().at(0).path));
        QVERIFY(bank.find(60, QStringLiteral("theirs")));
    }

    void a_directory_added_on_disk_is_read_with_everything_in_it() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("Shift_JIS"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        write(QStringLiteral("new/deeper/oto.ini"), "ka.wav=" + kShiftJisA + ",1,2,3,4,5\r\n");
        write(QStringLiteral("new/deeper/ka.wav"), "RIFF");
        DiagnosticList diagnostics;
        const auto done = disk.reloadFromDisk(bank, disk.checkDisk(), &selector, diagnostics);
        QCOMPARE(done.added, (QList<fs::path>{fs::path("new"), fs::path("new/deeper")}));

        const auto *sample = bank.find(60, kA);
        QVERIFY(sample);
        QCOMPARE(directoryOf(bank, *sample).path, fs::path("new/deeper"));
        QVERIFY(disk.checkDisk().isEmpty());
    }

    // The remaining samples still refer to the correct directories, although the indices
    // changed.
    void a_directory_removed_on_disk_goes_and_the_rest_stays_right() {
        write(QStringLiteral("one/oto.ini"), "a.wav=one,1,0,0,0,0\r\n");
        write(QStringLiteral("one/a.wav"), "RIFF");
        write(QStringLiteral("two/oto.ini"), "b.wav=two,2,0,0,0,0\r\n");
        write(QStringLiteral("two/b.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        QVERIFY(QDir(pathOf(QStringLiteral("one"))).removeRecursively());
        QCOMPARE(takeIn(bank, disk, &selector).removed, QList<fs::path>{fs::path("one")});
        QVERIFY(!bank.find(60, QStringLiteral("one")));

        const auto *sample = bank.find(60, QStringLiteral("two"));
        QVERIFY(sample);
        QCOMPARE(directoryOf(bank, *sample).path, fs::path("two"));
        QCOMPARE(sample->offset, 2.0);
        QVERIFY(disk.checkDisk().isEmpty());
    }

    // Only the parent directory is examined, and the removal appears in its listing.
    void a_removed_directory_appears_in_the_listing_of_its_parent() {
        write(QStringLiteral("one/a.wav"), "RIFF");
        write(QStringLiteral("two/b.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &disk = opened->disk;

        QVERIFY(QDir(pathOf(QStringLiteral("one"))).removeRecursively());
        QCOMPARE(disk.checkDisk({root() / "elsewhere"}).removed, QList<fs::path>{fs::path("one")});
    }

    // A watcher reports the new directory itself, which is not yet part of the voice bank. Its
    // parent is examined, where the new directory appears.
    void a_new_place_is_found_through_its_parent() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        write(QStringLiteral("x/y/ka.wav"), "RIFF");
        const auto found = takeIn(bank, disk, &selector, {root() / "x" / "y"});
        QCOMPARE(found.added, QList<fs::path>{fs::path("x")});
        QVERIFY(bank.find(60, QStringLiteral("ka")));
    }

    // A place only limits the examination. A change elsewhere is not detected until that
    // location is examined, which is why the entire voice bank must also be checked
    // periodically.
    void a_place_limits_the_examination() {
        write(QStringLiteral("sub/a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &disk = opened->disk;

        // In the root, which contains one place and is a sibling of the other.
        write(QStringLiteral("ka.wav"), "RIFF");
        QVERIFY(disk.checkDisk({root() / "sub"}).isEmpty());
        QVERIFY(disk.checkDisk({m_dir->path().toStdU16String()}).isEmpty());
        QCOMPARE(disk.checkDisk().audio, QList<fs::path>{fs::path()});
    }

    void a_root_that_is_not_found_is_reported_without_other_changes() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        QVERIFY(QDir().rename(pathOf(QString()), m_dir->path() + QStringLiteral("/moved")));
        QVERIFY(disk.checkDisk().rootNotFound);
        QVERIFY(bank.find(60, QStringLiteral("a")));
    }

    // The user is asked once on open. Asking again on every change would repeat an answered
    // question.
    void a_directory_read_before_is_not_asked_about_again() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QCOMPARE(selector.asked, 1);

        write(QStringLiteral("oto.ini"), "a.wav=b,1,2,3,4,5,changed\r\n");
        takeIn(bank, disk, &selector);
        QCOMPARE(selector.asked, 1);
        QVERIFY(bank.find(60, QStringLiteral("b")));
    }

    // The directory previously required no encoding, so the user was not asked. Now it
    // requires one.
    void a_directory_that_now_needs_an_encoding_is_asked_about() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QCOMPARE(selector.asked, 0);

        write(QStringLiteral("oto.ini"), "a.wav=first,1,2,3,4,5\r\n");
        takeIn(bank, disk, &selector);
        QCOMPARE(selector.asked, 1);
        QVERIFY(bank.find(60, QStringLiteral("first")));
    }

    // Another program, or another instance of this one, may write the configuration. Its
    // encoding takes precedence over the encoding in which the directory was read.
    void an_encoding_recorded_on_disk_is_the_one_read_in() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("GBK"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(!bank.find(60, kA));

        write(QStringLiteral("hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"Shift_JIS"})");
        takeIn(bank, disk, &selector);
        QVERIFY(bank.find(60, kA));
    }

    // The disk changed between the check and the reload. A directory removed since is not read,
    // and a directory restored since is not dropped.
    void a_reload_looks_again_at_what_the_check_found() {
        write(QStringLiteral("one/a.wav"), "RIFF");
        write(QStringLiteral("two/b.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        QVERIFY(QDir().rename(pathOf(QStringLiteral("one")), pathOf(QStringLiteral("away"))));
        write(QStringLiteral("new/c.wav"), "RIFF");
        const auto found = disk.checkDisk();
        QVERIFY(found.removed.contains(fs::path("one")));
        QVERIFY(found.added.contains(fs::path("new")));

        QVERIFY(QDir().rename(pathOf(QStringLiteral("away")), pathOf(QStringLiteral("one"))));
        QVERIFY(QDir(pathOf(QStringLiteral("new"))).removeRecursively());

        DiagnosticList diagnostics;
        const auto done = disk.reloadFromDisk(bank, found, &selector, diagnostics);
        QVERIFY(!done.removed.contains(fs::path("one")));
        QVERIFY(!done.added.contains(fs::path("new")));
        QVERIFY(bank.find(60, QStringLiteral("a")));
        QVERIFY(!bank.find(60, QStringLiteral("c")));
    }

    // The full refresh for cases where something appears wrong: everything is reread regardless
    // of the stamps. Here the stamp is deliberately defeated by restoring an old modification
    // time after a write of the same size, which a check trusts once the time is old enough.
    void reloading_everything_reads_what_a_stamp_would_miss() {
        write(QStringLiteral("oto.ini"), "a.wav=aaa,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        const auto oto = root() / "oto.ini";
        const auto old = fs::last_write_time(oto) - std::chrono::hours(1);
        fs::last_write_time(oto, old);

        CountingSelector selector(QStringLiteral("UTF-8"));
        auto opened = open(&selector);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        write(QStringLiteral("oto.ini"), "a.wav=bbb,1,2,3,4,5\r\n");
        fs::last_write_time(oto, old);
        QVERIFY(disk.checkDisk().isEmpty());

        DiagnosticList diagnostics;
        const auto done = disk.reloadAllFromDisk(bank, &selector, diagnostics);
        QVERIFY(done.changed.contains(fs::path()));
        QVERIFY(bank.find(60, QStringLiteral("bbb")));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankDiskState_Changes)

#include "test_VoiceBankDiskState_Changes.moc"
