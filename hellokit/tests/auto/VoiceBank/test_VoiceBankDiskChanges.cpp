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

    /// Answers every question with one encoding, and counts them.
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

    /// What a user who always says yes to reloading would get.
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

    // Whether to take in what changed is the user's to say, so a check only says.
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

    // A resampler writes its analysis beside the sample while rendering. That is not an edit,
    // and a render must not make the bank look changed.
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

    // Two writes inside one tick of the file system's clock look the same by size and time.
    // Such a file is compared by what it holds, or the second write would never be seen.
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

    // What was found and not taken in is found again, so a check whose answer was missed loses
    // nothing. The root's own files stay as they were here, so that nothing but its listing
    // tells of what came and went, and it is the root's stamp that must not move on.
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

    // Changed on disk and changed here: two versions, and the user's to choose. Nothing here
    // decides for them, and a reload is them choosing the disk's.
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

    // What is left keeps pointing at the right directory, though the indices moved.
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

    // Only the directory above is looked at, and what went shows in its listing.
    void a_directory_gone_shows_in_the_listing_above() {
        write(QStringLiteral("one/a.wav"), "RIFF");
        write(QStringLiteral("two/b.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        QVERIFY(QDir(pathOf(QStringLiteral("one"))).removeRecursively());
        QCOMPARE(bank->checkDisk({root() / "elsewhere"}).removed, QList<fs::path>{fs::path("one")});
    }

    // A watcher names the new directory itself, which the bank does not know. The directory
    // above it is looked at, which is where the new one shows.
    void a_place_the_bank_does_not_know_is_found_from_above() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        write(QStringLiteral("x/y/ka.wav"), "RIFF");
        const auto found = takeIn(*bank, &selector, {root() / "x" / "y"});
        QCOMPARE(found.added, QList<fs::path>{fs::path("x")});
        QVERIFY(bank->find(60, QStringLiteral("ka")));
    }

    // A place is only where to look. A change elsewhere is not seen until something looks
    // there, which is why the whole bank has to be looked at now and then as well.
    void a_place_says_where_to_look_and_nothing_more() {
        write(QStringLiteral("sub/a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        // In the root, which the one place is under and the other beside.
        write(QStringLiteral("ka.wav"), "RIFF");
        QVERIFY(bank->checkDisk({root() / "sub"}).isEmpty());
        QVERIFY(bank->checkDisk({m_dir->path().toStdU16String()}).isEmpty());
        QCOMPARE(bank->checkDisk().changed, QList<fs::path>{fs::path()});
    }

    void a_root_that_is_gone_is_said_and_nothing_else_happens() {
        write(QStringLiteral("a.wav"), "RIFF");
        CountingSelector selector(QStringLiteral("UTF-8"));
        auto bank = open(&selector);
        QVERIFY(bank.has_value());

        QVERIFY(QDir().rename(pathOf(QString()), m_dir->path() + QStringLiteral("/moved")));
        QVERIFY(bank->checkDisk().rootGone);
        QVERIFY(bank->find(60, QStringLiteral("a")));
    }

    // Asked once when opened. Asking again on every change would ask what was answered.
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

    // Nothing needed an encoding there before, so nothing was asked. Now something does.
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

    // Another program, or another copy of this one, may write the record. What it says wins
    // over what the directory was read in.
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

    // The disk moved on between the check and the reload. What went since is not read, and
    // what came back is not dropped.
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

    // The button for when something looks wrong: everything read again, whatever a stamp
    // says. Here the stamp is fooled on purpose, an old time put back after a write of the
    // same size, which a check trusts once the time is old enough.
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
