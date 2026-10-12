#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/VoiceBankFileSystemState.h>

using namespace hello::kit;

namespace fs = std::filesystem;

using FileChange = FrequencyFormat::FileChange;
using NameChanges = VoiceBankFileSystemState::NameChanges;

class test_VoiceBankFileSystemState_Rename : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    fs::path root() const {
        return fs::path(m_dir->path().toStdU16String());
    }

    static fs::path pathOf(const char16_t *text) {
        return fs::path(text);
    }

    void write(const fs::path &relative, const QByteArray &bytes) {
        const auto path = root() / relative;
        QVERIFY(QDir().mkpath(QString::fromStdU16String(path.parent_path().u16string())));
        QFile file(QString::fromStdU16String(path.u16string()));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    QByteArray read(const fs::path &relative) const {
        QFile file(QString::fromStdU16String((root() / relative).u16string()));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    bool exists(const fs::path &relative) const {
        std::error_code error;
        return fs::exists(root() / relative, error);
    }

    /// A Japanese voice bank extracted on a Chinese system: 偁.wav (あ) with its dio in the root,
    /// and the folder 偆 (う) with 偄.wav (い) and its frq.
    void writeBank() {
        write(pathOf(u"偁.wav"), "a");
        write(pathOf(u"偁.dio"), "a.dio");
        write(pathOf(u"偆/偄.wav"), "i");
        write(pathOf(u"偆/偄_wav.frq"), "i.frq");
    }

    /// The conversion of every name of writeBank().
    static QList<NameChanges> conversion() {
        return {
            {{},
             {{pathOf(u"偁.wav"), pathOf(u"あ.wav")}, {pathOf(u"偆"), pathOf(u"う")}},
             {FileChange::renamed(pathOf(u"偁.dio"), pathOf(u"あ.dio"))}        },
            {pathOf(u"偆"),
             {{pathOf(u"偄.wav"), pathOf(u"い.wav")}},
             {FileChange::renamed(pathOf(u"偄_wav.frq"), pathOf(u"い_wav.frq"))}},
        };
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    // The folder 偆 is changed before the root renames it, so that its path holds until then.
    void the_names_and_the_files_that_follow_them_are_renamed() {
        writeBank();
        DiagnosticList diagnostics;
        QVERIFY(VoiceBankFileSystemState::renameOnDisk(root(), conversion(), diagnostics));
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(read(pathOf(u"あ.wav")), QByteArray("a"));
        QCOMPARE(read(pathOf(u"あ.dio")), QByteArray("a.dio"));
        QCOMPARE(read(pathOf(u"う/い.wav")), QByteArray("i"));
        QCOMPARE(read(pathOf(u"う/い_wav.frq")), QByteArray("i.frq"));
        QVERIFY(!exists(pathOf(u"偁.wav")));
        QVERIFY(!exists(pathOf(u"偆")));
        QCOMPARE(QDir(m_dir->path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 3);
    }

    // The names pass through temporary names, and a rewrite follows the renames.
    void two_files_exchange_their_names_and_a_file_is_rewritten() {
        write("a.wav", "a");
        write("b.wav", "b");
        write("desc.mrq", "old");
        DiagnosticList diagnostics;
        QVERIFY(VoiceBankFileSystemState::renameOnDisk(
            root(),
            {
                {{},
                 {{"a.wav", "b.wav"}, {"b.wav", "a.wav"}},
                 {FileChange::rewritten("desc.mrq", "new")}}
        },
            diagnostics));
        QCOMPARE(read("a.wav"), QByteArray("b"));
        QCOMPARE(read("b.wav"), QByteArray("a"));
        QCOMPARE(read("desc.mrq"), QByteArray("new"));
    }

    // Every check precedes the first change, so a refusal leaves the disk as it was.
    void a_refused_conversion_changes_nothing() {
        QFETCH(QList<NameChanges>, changes);
        writeBank();
        write("c.wav", "c");
        write("e.txt", "e");
        DiagnosticList diagnostics;
        QVERIFY(
            !VoiceBankFileSystemState::renameOnDisk(root(), conversion() + changes, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
        QVERIFY(hasError(diagnostics));
        QCOMPARE(read(pathOf(u"偁.wav")), QByteArray("a"));
        QCOMPARE(read(pathOf(u"偆/偄.wav")), QByteArray("i"));
        QVERIFY(!exists(pathOf(u"あ.wav")));
    }

    void a_refused_conversion_changes_nothing_data() {
        QTest::addColumn<QList<NameChanges>>("changes");
        const auto renames = [](fs::path from, fs::path to) {
            return QList<NameChanges>{
                {{}, {{std::move(from), std::move(to)}}, {}}
            };
        };
        QTest::newRow("existing name") << renames("c.wav", "e.txt");
        QTest::newRow("name that another rename receives") << renames("c.wav", pathOf(u"あ.wav"));
        QTest::newRow("missing name") << renames("d.wav", "e.wav");
        QTest::newRow("separator") << renames("c.wav", "x/y.wav");
        QTest::newRow("colon") << renames("c.wav", "x:y.wav");
        QTest::newRow("trailing dot") << renames("c.wav", "x.");
        QTest::newRow("missing folder") << QList<NameChanges>{
            {"none", {{"a.wav", "b.wav"}}, {}}
        };
        QTest::newRow("missing rewrite") << QList<NameChanges>{
            {{}, {}, {FileChange::rewritten("desc.mrq", "x")}}
        };
    }

    // Two names that become one name are refused as well, which the list of the directory alone
    // would not show.
    void two_files_cannot_receive_one_name() {
        write("a.wav", "a");
        write("b.wav", "b");
        DiagnosticList diagnostics;
        QVERIFY(!VoiceBankFileSystemState::renameOnDisk(
            root(),
            {
                {{}, {{"a.wav", "c.wav"}, {"b.wav", "c.wav"}}, {}}
        },
            diagnostics));
        QVERIFY(exists("a.wav") && exists("b.wav") && !exists("c.wav"));
    }

    // A change that fails after the checks, here the rewrite of a read-only file, undoes
    // every change made before it.
    void a_failure_partway_through_changes_every_name_back() {
        writeBank();
        write("desc.mrq", "old");
        const auto desc = QString::fromStdU16String((root() / "desc.mrq").u16string());
        QVERIFY(QFile::setPermissions(desc, QFile::ReadOwner));
        auto changes = conversion();
        changes.first().files.push_back(FileChange::rewritten("desc.mrq", "new"));
        DiagnosticList diagnostics;
        const bool renamed = VoiceBankFileSystemState::renameOnDisk(root(), changes, diagnostics);
        QFile::setPermissions(desc, QFile::ReadOwner | QFile::WriteOwner);
        QVERIFY(!renamed);
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(read(pathOf(u"偁.wav")), QByteArray("a"));
        QCOMPARE(read(pathOf(u"偁.dio")), QByteArray("a.dio"));
        QCOMPARE(read(pathOf(u"偆/偄.wav")), QByteArray("i"));
        QCOMPARE(read(pathOf(u"偆/偄_wav.frq")), QByteArray("i.frq"));
        QCOMPARE(read("desc.mrq"), QByteArray("old"));
        QCOMPARE(QDir(m_dir->path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 4);
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankFileSystemState_Rename)

#include "test_VoiceBankFileSystemState_Rename.moc"
