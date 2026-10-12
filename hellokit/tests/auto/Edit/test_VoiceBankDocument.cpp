#include <memory>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Edit/VoiceBankDocument.h>
#include <hellokit/Edit/VoiceBankRefs.h>

#include "VoiceBankSamples.h"

using namespace hello::kit;

namespace fs = std::filesystem;

// The saved state of a voice bank opened for editing, see the section on the saved state in
// docs/Editing.md.
class test_VoiceBankDocument : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    fs::path root() const {
        return fs::path(m_dir->path().toStdU16String());
    }

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

    // A voice bank whose root is in GBK, recorded unless chosen is true, in which case the
    // encoding is chosen on opening.
    std::unique_ptr<VoiceBankDocument> open(bool chosen = false) {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        if (!chosen) {
            write(QStringLiteral("hello-config.json"),
                  R"({"$format": "hello-voicebank", "charset": "GBK"})");
        }
        DirectorySelector selector({
            {fs::path(), QStringLiteral("GBK")},
        });
        DiagnosticList diagnostics;
        return VoiceBankDocument::open(root(), &selector, diagnostics);
    }

    static void rename(VoiceBankDocument &document, const QString &alias) {
        auto transaction = document.session()->transaction(QStringLiteral("rename"));
        VoiceBankRef(document.session()).directories().at(0).otoEntries().at(0).setAlias(alias);
        QVERIFY(transaction.commit());
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void a_recorded_voice_bank_opens_unmodified_and_follows_its_steps() {
        const auto document = open();
        QVERIFY(document);
        QVERIFY(!document->isModified());
        QCOMPARE(document->displayName(), QDir(m_dir->path()).dirName());

        QSignalSpy spy(document.get(), &VoiceBankDocument::modifiedChanged);
        rename(*document, QStringLiteral("b"));
        QVERIFY(document->isModified());
        document->session()->undo();
        QVERIFY(!document->isModified());
        QCOMPARE(spy.count(), 2);

        // A step that takes the number of the saved one is not the saved state.
        document->session()->redo();
        QSignalSpy saved(document.get(), &VoiceBankDocument::saved);
        DiagnosticList diagnostics;
        QVERIFY(document->save(diagnostics));
        QVERIFY(!document->isModified());
        QCOMPARE(saved.count(), 1);
        document->session()->undo();
        rename(*document, QStringLiteral("c"));
        QCOMPARE(document->session()->currentStep(), 1);
        QVERIFY(document->isModified());

        // A save that fails, into a root that is a file now, reports none.
        QVERIFY(QDir(m_dir->path()).removeRecursively());
        QFile blocker(m_dir->path());
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QVERIFY(!document->save(diagnostics));
        QCOMPARE(saved.count(), 1);
        QVERIFY(QFile::remove(m_dir->path()));
    }

    // An encoding chosen on opening is recorded by the next save, which is needed even
    // without an edit.
    void a_chosen_encoding_leaves_the_voice_bank_modified_until_saved() {
        const auto document = open(true);
        QVERIFY(document);
        QVERIFY(document->isModified());
        QVERIFY(!QFile::exists(pathOf(QStringLiteral("hello-config.json"))));

        DiagnosticList diagnostics;
        QVERIFY(document->save(diagnostics));
        QVERIFY(!document->isModified());
        QVERIFY(QFile::exists(pathOf(QStringLiteral("hello-config.json"))));
    }

    // A text file changed on disk leaves the voice bank modified, also after it is read again;
    // a change of the audio files alone does not.
    void a_change_on_disk_makes_the_voice_bank_modified() {
        const auto document = open();
        write(QStringLiteral("b.wav"), "RIFF");
        auto changes = document->checkDisk();
        QCOMPARE(changes.audio.size(), 1);
        QVERIFY(!document->isModified());

        write(QStringLiteral("oto.ini"), "a.wav=changed,1,2,3,4,5\r\n");
        changes = document->checkDisk();
        QCOMPARE(changes.changed.size(), 1);
        QVERIFY(document->isModified());

        DiagnosticList diagnostics;
        document->reloadFromDisk(changes, nullptr, diagnostics);
        QCOMPARE(VoiceBankRef(document->session()).directories().at(0).otoEntries().at(0).alias(),
                 QStringLiteral("changed"));
        QVERIFY(document->isModified());
        document->session()->undo();
        QVERIFY(document->isModified());
        document->session()->redo();
        QVERIFY(document->save(diagnostics));
        QVERIFY(!document->isModified());
    }

    // File names determined on opening are recorded by the next save, so the voice bank is
    // unsaved until then, although nothing was edited.
    void file_names_determined_on_opening_are_unsaved_until_saved() {
        VoiceBankConfiguration config;
        config.charset = QStringLiteral("Shift_JIS");
        write(QStringLiteral("hello-config.json"), config.toJson());
        write(QStringLiteral("oto.ini"), "\x82\xa0.wav=a,1,2,3,4,5\r\n");
        write(QString::fromUtf8("偁.wav"), "RIFF");
        AcceptingSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        const auto document = VoiceBankDocument::open(root(), &selector, diagnostics);
        QVERIFY(document);
        QVERIFY(document->session()->hasUnrecordedCharsets());
        QVERIFY(document->isModified());

        QVERIFY(document->save(diagnostics));
        QVERIFY(!document->isModified());
        DiagnosticList ignored;
        const auto saved = VoiceBankConfiguration::open(root() / "hello-config.json", ignored);
        QVERIFY(saved.has_value());
        QVERIFY(saved->fileNameEncoding.has_value());
        QCOMPARE(saved->fileNameEncoding->logicalNameOf(QString::fromUtf8("偁.wav")),
                 QString::fromUtf8("あ.wav"));
    }

    // The configuration is read on opening only, so a change by another program is not reported
    // and leaves the voice bank saved.
    void a_changed_configuration_leaves_the_voice_bank_saved() {
        const auto document = open();
        write(QStringLiteral("hello-config.json"),
              R"({"$format": "hello-voicebank", "charset": "UTF-8"})");
        QVERIFY(document->checkDisk().isEmpty());
        QVERIFY(!document->isModified());
    }

    // The root removed on disk leaves the tree as the only copy.
    void a_removed_root_makes_the_voice_bank_modified() {
        const auto document = open();
        QVERIFY(QDir(m_dir->path()).removeRecursively());
        const auto changes = document->checkDisk();
        QVERIFY(changes.rootNotFound);
        QVERIFY(document->isModified());
    }

    void saving_as_moves_the_voice_bank_to_the_new_folder() {
        const auto document = open();
        rename(*document, QStringLiteral("b"));
        QSignalSpy moved(document.get(), &VoiceBankDocument::rootPathChanged);
        QSignalSpy saved(document.get(), &VoiceBankDocument::saved);
        QTemporaryDir other;
        const auto folder = fs::path(other.path().toStdU16String()) / "copy";
        DiagnosticList diagnostics;
        QVERIFY(document->saveAs(folder, VoiceBankSession::AllFiles, diagnostics));
        QCOMPARE(moved.count(), 1);
        QCOMPARE(saved.count(), 1);
        QCOMPARE(document->rootPath(), folder);
        QCOMPARE(document->displayName(), QStringLiteral("copy"));
        QVERIFY(!document->isModified());
    }

    void a_voice_bank_that_does_not_exist_is_not_opened() {
        DiagnosticList diagnostics;
        QVERIFY(!VoiceBankDocument::open(root() / "missing", nullptr, diagnostics));
        QVERIFY(!diagnostics.isEmpty());
    }

    // The edits not yet saved are saved before the names change on disk, so that the voice bank
    // read again holds them.
    void the_edits_are_saved_before_the_names_change() {
        write(QString::fromUtf8("偁.wav"), "RIFF");
        const auto document = open();
        QVERIFY(document);
        rename(*document, QStringLiteral("b"));
        QVERIFY(document->isModified());
        VoiceBankConfiguration::FileNameEncoding record;
        record.original = QStringLiteral("Shift_JIS");
        record.misdecodedAs = QStringLiteral("GBK");
        DiagnosticList diagnostics;
        QVERIFY(document->renameOnDisk(
            {
                {{}, record, {QString::fromUtf8("偁.wav")}}
        },
            nullptr, diagnostics));
        QFile oto(pathOf(QStringLiteral("oto.ini")));
        QVERIFY(oto.open(QIODevice::ReadOnly));
        QVERIFY(oto.readAll().contains("a.wav=b,"));
        QVERIFY(QFile::exists(pathOf(QString::fromUtf8("あ.wav"))));
        QVERIFY(!QFile::exists(pathOf(QString::fromUtf8("偁.wav"))));
    }
};

QTEST_GUILESS_MAIN(test_VoiceBankDocument)

#include "test_VoiceBankDocument.moc"
