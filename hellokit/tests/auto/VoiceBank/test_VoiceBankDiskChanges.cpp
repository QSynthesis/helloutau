#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/VoiceBank.h>

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
    VoiceBankChanges takeIn(VoiceBank &bank, VoiceBankCharsetSelector *selector,
                            const QList<fs::path> &places = {}) {
        DiagnosticList diagnostics;
        const auto found = places.isEmpty() ? bank.checkDisk() : bank.checkDisk(places);
        bank.reloadFromDisk(found, selector, diagnostics);
        return found;
    }

}

class test_VoiceBankDiskChanges : public QObject {
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

    std::optional<VoiceBank> open(VoiceBankCharsetSelector *selector) const {
        DiagnosticList diagnostics;
        return VoiceBank::open(root(), selector, diagnostics);
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
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        write(QStringLiteral("oto.ini"), "a.wav=newer,10,2,3,4,5\r\n");
        const auto found = bank->checkDisk();
        QCOMPARE(found.changed, QList<fs::path>{fs::path()});
        QVERIFY(bank->find(60, QStringLiteral("old")));
        QVERIFY(!bank->find(60, QStringLiteral("newer")));

        DiagnosticList diagnostics;
        bank->reloadFromDisk(found, &selector, diagnostics);
        QVERIFY(!bank->find(60, QStringLiteral("old")));
        const auto *sample = bank->find(60, QStringLiteral("newer"));
        QVERIFY(sample);
        QCOMPARE(sample->offset, 10.0);
        QVERIFY(bank->checkDisk().isEmpty());
    }

    void a_sample_added_on_disk_is_found() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        write(QStringLiteral("ka.wav"), "RIFF");
        QCOMPARE(takeIn(*bank, &selector).changed, QList<fs::path>{fs::path()});
        QVERIFY(bank->find(60, QStringLiteral("ka")));
    }

    void a_sample_removed_on_disk_is_dropped() {
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("ka.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        QVERIFY(QFile::remove(pathOf(QStringLiteral("ka.wav"))));
        takeIn(*bank, &selector);
        QVERIFY(!bank->find(60, QStringLiteral("ka")));
        QVERIFY(bank->find(60, QStringLiteral("a")));
    }

    // A resampler writes its analysis file beside the sample during rendering. This is not an
    // edit, and rendering must not make the voice bank appear modified.
    void what_a_render_leaves_beside_a_sample_changes_nothing() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        write(QStringLiteral("a_wav.frq"), "FREQ");
        write(QStringLiteral("a.llsm"), "LLSM");
        QVERIFY(bank->checkDisk().isEmpty());
    }

    // The character.txt, prefix.map and readme.txt of a subdirectory are not part of this voice
    // bank, so changing them is not a change of it. The same files in the root are.
    void the_text_files_of_a_subdirectory_are_not_watched() {
        write(QStringLiteral("inner/oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("inner/a.wav"), "RIFF");
        write(QStringLiteral("inner/character.txt"), "name=inner\n");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        write(QStringLiteral("inner/character.txt"), "name=changed inner\n");
        write(QStringLiteral("inner/prefix.map"), "C4\t\t_B\n");
        write(QStringLiteral("inner/readme.txt"), "readme");
        QVERIFY(bank->checkDisk().isEmpty());

        write(QStringLiteral("character.txt"), "name=root\n");
        QCOMPARE(bank->checkDisk().changed, QList<fs::path>{fs::path()});
    }

    // Two writes within one timestamp interval of the file system are indistinguishable by size
    // and time. Such a file is compared by content, because otherwise the second write would
    // never be detected.
    void a_change_inside_one_tick_of_the_clock_is_still_seen() {
        write(QStringLiteral("oto.ini"), "a.wav=aaa,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        const auto oto = root() / "oto.ini";
        const auto time = fs::last_write_time(oto);
        write(QStringLiteral("oto.ini"), "a.wav=bbb,1,2,3,4,5\r\n");
        fs::last_write_time(oto, time);

        QCOMPARE(takeIn(*bank, &selector).changed, QList<fs::path>{fs::path()});
        QVERIFY(bank->find(60, QStringLiteral("bbb")));
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
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        write(QStringLiteral("kept/oto.ini"), "k.wav=changed,1,2,3,4,5\r\n");
        write(QStringLiteral("coming/c.wav"), "RIFF");
        QVERIFY(QDir(pathOf(QStringLiteral("going"))).removeRecursively());

        for (int i = 0; i < 3; ++i) {
            const auto found = bank->checkDisk();
            QCOMPARE(found.changed, QList<fs::path>{fs::path("kept")});
            QCOMPARE(found.added, QList<fs::path>{fs::path("coming")});
            QCOMPARE(found.removed, QList<fs::path>{fs::path("going")});
        }
        QVERIFY(bank->find(60, QStringLiteral("k")));
    }

    // Changed on disk and modified in memory: two versions, between which the user must
    // choose. Nothing here decides on the user's behalf, and a reload represents choosing the
    // version on disk.
    void a_change_on_disk_leaves_one_not_saved_alone_until_the_user_chooses() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        auto samples = bank->samples();
        samples[0].offset = 99;
        bank->setSamples(samples);

        write(QStringLiteral("oto.ini"), "a.wav=theirs,7,2,3,4,5\r\n");
        const auto found = bank->checkDisk();
        QCOMPARE(found.changed, QList<fs::path>{fs::path()});
        QVERIFY(bank->isModified(0));
        QCOMPARE(bank->samples().at(0).offset, 99.0);

        DiagnosticList diagnostics;
        bank->reloadFromDisk(found, &selector, diagnostics);
        QVERIFY(!bank->isModified(0));
        QVERIFY(bank->find(60, QStringLiteral("theirs")));
    }

    void a_directory_added_on_disk_is_read_with_everything_in_it() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("Shift_JIS"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        write(QStringLiteral("new/deeper/oto.ini"), "ka.wav=" + kShiftJisA + ",1,2,3,4,5\r\n");
        write(QStringLiteral("new/deeper/ka.wav"), "RIFF");
        DiagnosticList diagnostics;
        const auto done = bank->reloadFromDisk(bank->checkDisk(), &selector, diagnostics);
        QCOMPARE(done.added, (QList<fs::path>{fs::path("new"), fs::path("new/deeper")}));

        const auto *sample = bank->find(60, kA);
        QVERIFY(sample);
        QCOMPARE(directoryOf(*bank, *sample).path, fs::path("new/deeper"));
        QVERIFY(bank->checkDisk().isEmpty());
    }

    // The remaining samples still refer to the correct directories, although the indices
    // changed.
    void a_directory_removed_on_disk_goes_and_the_rest_stays_right() {
        write(QStringLiteral("one/oto.ini"), "a.wav=one,1,0,0,0,0\r\n");
        write(QStringLiteral("one/a.wav"), "RIFF");
        write(QStringLiteral("two/oto.ini"), "b.wav=two,2,0,0,0,0\r\n");
        write(QStringLiteral("two/b.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        QVERIFY(QDir(pathOf(QStringLiteral("one"))).removeRecursively());
        QCOMPARE(takeIn(*bank, &selector).removed, QList<fs::path>{fs::path("one")});
        QVERIFY(!bank->find(60, QStringLiteral("one")));

        const auto *sample = bank->find(60, QStringLiteral("two"));
        QVERIFY(sample);
        QCOMPARE(directoryOf(*bank, *sample).path, fs::path("two"));
        QCOMPARE(sample->offset, 2.0);
        QVERIFY(bank->checkDisk().isEmpty());
    }

    // Only the parent directory is examined, and the removal appears in its listing.
    void a_removed_directory_appears_in_the_listing_of_its_parent() {
        write(QStringLiteral("one/a.wav"), "RIFF");
        write(QStringLiteral("two/b.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        QVERIFY(QDir(pathOf(QStringLiteral("one"))).removeRecursively());
        QCOMPARE(bank->checkDisk({root() / "elsewhere"}).removed, QList<fs::path>{fs::path("one")});
    }

    // A watcher reports the new directory itself, which is not yet part of the voice bank. Its
    // parent is examined, where the new directory appears.
    void a_new_place_is_found_through_its_parent() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        write(QStringLiteral("x/y/ka.wav"), "RIFF");
        const auto found = takeIn(*bank, &selector, {root() / "x" / "y"});
        QCOMPARE(found.added, QList<fs::path>{fs::path("x")});
        QVERIFY(bank->find(60, QStringLiteral("ka")));
    }

    // A place only limits the examination. A change elsewhere is not detected until that
    // location is examined, which is why the entire voice bank must also be checked
    // periodically.
    void a_place_limits_the_examination() {
        write(QStringLiteral("sub/a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        // In the root, which contains one place and is a sibling of the other.
        write(QStringLiteral("ka.wav"), "RIFF");
        QVERIFY(bank->checkDisk({root() / "sub"}).isEmpty());
        QVERIFY(bank->checkDisk({m_dir->path().toStdU16String()}).isEmpty());
        QCOMPARE(bank->checkDisk().changed, QList<fs::path>{fs::path()});
    }

    void a_root_that_is_not_found_is_reported_without_other_changes() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        QVERIFY(QDir().rename(pathOf(QString()), m_dir->path() + QStringLiteral("/moved")));
        QVERIFY(bank->checkDisk().rootNotFound);
        QVERIFY(bank->find(60, QStringLiteral("a")));
    }

    // The user is asked once on open. Asking again on every change would repeat an answered
    // question.
    void a_directory_read_before_is_not_asked_about_again() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());
        QCOMPARE(selector.asked, 1);

        write(QStringLiteral("oto.ini"), "a.wav=b,1,2,3,4,5,changed\r\n");
        takeIn(*bank, &selector);
        QCOMPARE(selector.asked, 1);
        QVERIFY(bank->find(60, QStringLiteral("b")));
    }

    // The directory previously required no encoding, so the user was not asked. Now it
    // requires one.
    void a_directory_that_now_needs_an_encoding_is_asked_about() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());
        QCOMPARE(selector.asked, 0);

        write(QStringLiteral("oto.ini"), "a.wav=first,1,2,3,4,5\r\n");
        takeIn(*bank, &selector);
        QCOMPARE(selector.asked, 1);
        QVERIFY(bank->find(60, QStringLiteral("first")));
    }

    // Another program, or another instance of this one, may write the configuration. Its
    // encoding takes precedence over the encoding in which the directory was read.
    void an_encoding_recorded_on_disk_is_the_one_read_in() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("GBK"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());
        QVERIFY(!bank->find(60, kA));

        write(QStringLiteral("hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"Shift_JIS"})");
        takeIn(*bank, &selector);
        QVERIFY(bank->find(60, kA));
    }

    // The disk changed between the check and the reload. A directory removed since is not read,
    // and a directory restored since is not dropped.
    void a_reload_looks_again_at_what_the_check_found() {
        write(QStringLiteral("one/a.wav"), "RIFF");
        write(QStringLiteral("two/b.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        QVERIFY(QDir().rename(pathOf(QStringLiteral("one")), pathOf(QStringLiteral("away"))));
        write(QStringLiteral("new/c.wav"), "RIFF");
        const auto found = bank->checkDisk();
        QVERIFY(found.removed.contains(fs::path("one")));
        QVERIFY(found.added.contains(fs::path("new")));

        QVERIFY(QDir().rename(pathOf(QStringLiteral("away")), pathOf(QStringLiteral("one"))));
        QVERIFY(QDir(pathOf(QStringLiteral("new"))).removeRecursively());

        DiagnosticList diagnostics;
        const auto done = bank->reloadFromDisk(found, &selector, diagnostics);
        QVERIFY(!done.removed.contains(fs::path("one")));
        QVERIFY(!done.added.contains(fs::path("new")));
        QVERIFY(bank->find(60, QStringLiteral("a")));
        QVERIFY(!bank->find(60, QStringLiteral("c")));
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
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        write(QStringLiteral("oto.ini"), "a.wav=bbb,1,2,3,4,5\r\n");
        fs::last_write_time(oto, old);
        QVERIFY(bank->checkDisk().isEmpty());

        DiagnosticList diagnostics;
        const auto done = bank->reloadAllFromDisk(&selector, diagnostics);
        QVERIFY(done.changed.contains(fs::path()));
        QVERIFY(bank->find(60, QStringLiteral("bbb")));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankDiskChanges)

#include "test_VoiceBankDiskChanges.moc"
