#include <memory>

#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QMetaMethod>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Edit/VoiceBankCommands.h>
#include <hellokit/Edit/VoiceBankEdits.h>
#include <hellokit/Edit/VoiceBankRefs.h>

#include "VoiceBankSamples.h"

using namespace hello::kit;

namespace fs = std::filesystem;

// The command lines are ordinary string literals rather than raw string literals, because moc
// does not recognize the class of a file whose raw strings contain unpaired quotes.
class test_VoiceBankCommands : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::optional<VoiceBank> m_bank;
    std::unique_ptr<VoiceBankSession> m_session;

    bool run(const QString &line) {
        DiagnosticList diagnostics;
        const auto executed = VoiceBankCommands::execute(*m_session, line, diagnostics);
        if (!executed) {
            qDebug().noquote() << line
                               << (diagnostics.isEmpty() ? QString() : diagnostics.first().message);
        }
        return executed;
    }

    // Verifies that line is refused with an error whose message contains reason, and leaves the
    // voice bank and the history unchanged.
    void verifyRefused(const QString &line, const QString &reason = QString()) {
        const auto step = m_session->currentStep();
        const auto before = m_session->snapshot();
        DiagnosticList diagnostics;
        QVERIFY2(!VoiceBankCommands::execute(*m_session, line, diagnostics), qPrintable(line));
        QVERIFY2(hasError(diagnostics), qPrintable(line));
        QVERIFY2(diagnostics.first().message.contains(reason),
                 qPrintable(diagnostics.first().message));
        QCOMPARE(m_session->currentStep(), step);
        verifyEqual(m_session->snapshot(), before);
    }

    OtoEntryListRef rootEntries() const {
        return VoiceBankRef(m_session.get()).directories().at(0).otoEntries();
    }

    QStringList rootFileNames() const {
        QStringList names;
        const auto entries = rootEntries();
        for (int i = 0; i < entries.size(); ++i) {
            names.push_back(entries.at(i).fileName());
        }
        return names;
    }

    // The names of the invokable functions declared in the class of meta, sorted.
    static QStringList invokableNames(const QMetaObject &meta) {
        QStringList names;
        for (int i = meta.methodOffset(); i < meta.methodCount(); ++i) {
            names.push_back(QString::fromLatin1(meta.method(i).name()));
        }
        names.sort();
        return names;
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        auto opened = openRichBank(m_dir->path());
        QVERIFY(opened.has_value());
        m_bank = editablePart(opened->bank);
        DiagnosticList diagnostics;
        m_session = VoiceBankSession::create(std::move(*opened), diagnostics);
        QVERIFY(m_session);
    }

    void cleanup() {
        m_session.reset();
        m_bank.reset();
        m_dir.reset();
    }

    // Each command is one undo step with the command as its message.
    void a_command_is_one_undo_step() {
        const auto line = QStringLiteral("set /directories/0/otoEntries/3/alias n");
        QVERIFY(run(line));
        QCOMPARE(rootEntries().at(3).alias(), QStringLiteral("n"));
        QCOMPARE(m_session->currentStep(), 1);
        QCOMPARE(m_session->undoMessage(), line);
    }

    // The facts of the disk and the encodings are not set by a node operation, and the spellings
    // are not addressed.
    void a_read_only_or_internal_field_is_refused() {
        verifyRefused(QStringLiteral("set /directories/0/charset \"UTF-8\""),
                      QStringLiteral("read-only"));
        verifyRefused(QStringLiteral("remove /directories 0"), QStringLiteral("read-only"));
        verifyRefused(QStringLiteral("set /directories/0/otoEntries/0/spellings []"),
                      QStringLiteral("has no field spellings"));
    }

    void an_entry_is_set_as_a_whole() {
        QVERIFY(run(QStringLiteral("entry set /directories/0/otoEntries/3 "
                                   "{\"fileName\": \"c.wav\", \"alias\": \"c\", \"offset\": 5}")));
        const auto entry = rootEntries().at(3).toVoiceOtoEntry();
        QCOMPARE(entry.fileName, QStringLiteral("c.wav"));
        QCOMPARE(entry.alias, QStringLiteral("c"));
        QCOMPARE(entry.offset, 5.0);
        QCOMPARE(entry.consonant, 0.0);

        verifyRefused(QStringLiteral("entry set /directories/0 {\"fileName\": \"c.wav\"}"),
                      QStringLiteral("does not denote an oto entry"));
        verifyRefused(QStringLiteral("entry set /directories/0/otoEntries/3 \"c.wav\""),
                      QStringLiteral("must be an object"));
        verifyRefused(
            QStringLiteral("entry set /directories/0/otoEntries/3 {\"fileName\": \"c.wav\", "
                           "\"spellings\": []}"),
            QStringLiteral("has no field spellings"));
        verifyRefused(QStringLiteral("entry set /directories/0/otoEntries/3 {\"alias\": \"c\"}"),
                      QStringLiteral("requires the field fileName"));
        verifyRefused(QStringLiteral("entry set /directories/0/otoEntries/3"),
                      QStringLiteral("Usage"));
    }

    void entries_are_inserted_included_and_removed() {
        QVERIFY(run(QStringLiteral("entry insert /directories/0 {\"fileName\": \"0.wav\"} "
                                   "{\"fileName\": \"z.wav\", \"alias\": \"z\"}")));
        QCOMPARE(rootFileNames(),
                 QStringList({QStringLiteral("0.wav"), QStringLiteral("a.wav"),
                              QStringLiteral("a.wav"), QStringLiteral("b.wav"),
                              QStringLiteral("missing.wav"), QStringLiteral("z.wav")}));

        QVERIFY(run(QStringLiteral("entry include /directories/0 c.wav")));
        QCOMPARE(rootFileNames().at(4), QStringLiteral("c.wav"));

        QVERIFY(run(QStringLiteral("entry remove /directories/0 0 6")));
        QCOMPARE(rootFileNames(), QStringList({QStringLiteral("a.wav"), QStringLiteral("a.wav"),
                                               QStringLiteral("b.wav"), QStringLiteral("c.wav"),
                                               QStringLiteral("missing.wav")}));

        verifyRefused(QStringLiteral("entry insert /directories/0 x"),
                      QStringLiteral("must be an object"));
        verifyRefused(QStringLiteral("entry insert /directories/0/otoEntries/0 {\"fileName\": "
                                     "\"x.wav\"}"),
                      QStringLiteral("does not denote a folder"));
        verifyRefused(QStringLiteral("entry include /directories/0 x.wav"),
                      QStringLiteral("is not an audio file"));
        verifyRefused(QStringLiteral("entry include /directories/0 1"),
                      QStringLiteral("must be a string"));
        verifyRefused(QStringLiteral("entry remove /directories/0 x"),
                      QStringLiteral("must be an integer"));
        verifyRefused(QStringLiteral("entry remove /directories/0 9"),
                      QStringLiteral("not an entry"));
        verifyRefused(QStringLiteral("entry remove /directories/0"), QStringLiteral("Usage"));
    }

    void a_prefix_is_set_and_removed() {
        QVERIFY(run(QStringLiteral("prefix set 61 {\"prefix\": \"x\"}")));
        const auto prefixMap = VoiceBankRef(m_session.get()).prefixMap();
        QCOMPARE(prefixMap.value(61).prefix, QStringLiteral("x"));
        QCOMPARE(prefixMap.value(61).suffix, QString());
        QVERIFY(run(QStringLiteral("prefix remove 60")));
        QCOMPARE(prefixMap.keys(), QList<int>({61, 62}));

        verifyRefused(QStringLiteral("prefix set 61 \"x\""), QStringLiteral("must be a prefix"));
        verifyRefused(QStringLiteral("prefix set 61 {\"infix\": \"x\"}"),
                      QStringLiteral("must be a prefix"));
        verifyRefused(QStringLiteral("prefix set x {}"), QStringLiteral("must be an integer"));
        verifyRefused(QStringLiteral("prefix set 23 {}"), QStringLiteral("not a note number"));
        verifyRefused(QStringLiteral("prefix remove 60"), QStringLiteral("has no key 60"));
        verifyRefused(QStringLiteral("prefix remove"), QStringLiteral("Usage"));
    }

    void an_encoding_is_converted() {
        const auto sub = m_bank->indexOf("sub");
        QVERIFY(sub > 0);
        QVERIFY(run(QStringLiteral("directory charset /directories/%1 gbk").arg(sub)));
        const auto directory = VoiceBankRef(m_session.get()).directories().at(sub);
        QCOMPARE(directory.charset(), QStringLiteral("GBK"));

        verifyRefused(QStringLiteral("directory charset /directories/0 no-such-encoding"),
                      QStringLiteral("is not available"));
        verifyRefused(QStringLiteral("directory charset /directories/0/otoEntries/0 gbk"),
                      QStringLiteral("does not denote a folder"));
        verifyRefused(QStringLiteral("directory charset /directories/0 1"),
                      QStringLiteral("must be a string"));
    }

    void an_unknown_command_is_refused() {
        verifyRefused(QStringLiteral("entry"), QStringLiteral("requires a verb"));
        verifyRefused(QStringLiteral("entry rename /directories/0/otoEntries/0 x"),
                      QStringLiteral("entry rename is not a command"));
        verifyRefused(QStringLiteral("prefix insert 60 {}"),
                      QStringLiteral("prefix insert is not a command"));
        verifyRefused(QStringLiteral("rename /readme x"), QStringLiteral("is not a command"));
        verifyRefused(QStringLiteral("\"entry\" set"), QStringLiteral("begins with its name"));
    }

    // Acceptance criterion 7 of docs/Editing.md: every domain function has a command, and every
    // domain command calls a domain function. The functions are those that the meta-object of
    // VoiceBankEdits lists.
    void every_domain_function_has_a_command() {
        const auto functions = VoiceBankCommands::domainFunctions();
        QCOMPARE(invokableNames(VoiceBankEdits::staticMetaObject), functions.keys());
        const auto names = VoiceBankCommands::names();
        for (const auto &command : functions) {
            QVERIFY2(names.contains(command), qPrintable(command));
        }
        for (const auto &name : names) {
            QVERIFY2(!name.contains(QLatin1Char(' ')) || functions.values().contains(name),
                     qPrintable(name));
        }
    }

    // The entries of a directory are read with their positions, which are the indices that the
    // commands take. The spellings are internal, and are neither returned nor addressed.
    void get_returns_the_entries_of_a_directory() {
        const auto get = [this](const char *line) {
            DiagnosticList diagnostics;
            return VoiceBankCommands::query(*m_session, QString::fromUtf8(line), diagnostics);
        };
        const auto entries = get("get /directories/0/otoEntries");
        QVERIFY(entries);
        const auto array = entries->toArray();
        QCOMPARE(array.size(), rootEntries().size());
        for (int i = 0; i < array.size(); ++i) {
            const auto entry = array.at(i).toObject();
            QCOMPARE(entry.value("fileName").toString(), rootEntries().at(i).fileName());
            QCOMPARE(entry.value("alias").toString(), rootEntries().at(i).alias());
            QVERIFY(!entry.contains("spellings"));
        }

        const auto directory = get("get /directories/0");
        QVERIFY(directory);
        const auto root = VoiceBankRef(m_session.get()).directories().at(0);
        QCOMPARE(directory->toObject().value("charset").toString(), root.charset());
        QCOMPARE(directory->toObject().value("otoEntries"), QJsonValue(array));
        QVERIFY(get("get /prefixMap")->toObject().contains("60"));
        QVERIFY(!get("get /directories/0/otoEntries/0/spellings"));
        QCOMPARE(m_session->currentStep(), 0);
    }

    void names_lists_every_command() {
        QCOMPARE(
            VoiceBankCommands::names(),
            QStringList({QStringLiteral("set"), QStringLiteral("insert"), QStringLiteral("remove"),
                         QStringLiteral("move"), QStringLiteral("replace"),
                         QStringLiteral("entry set"), QStringLiteral("entry insert"),
                         QStringLiteral("entry include"), QStringLiteral("entry remove"),
                         QStringLiteral("prefix set"), QStringLiteral("prefix remove"),
                         QStringLiteral("directory charset")}));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankCommands)

#include "test_VoiceBankCommands.moc"
