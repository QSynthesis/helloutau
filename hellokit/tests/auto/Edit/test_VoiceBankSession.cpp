#include <memory>

#include <QtCore/QJsonArray>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/Edit/VoiceBankSession.h>

#include "VoiceBankSamples.h"

using namespace hello::kit;

class test_VoiceBankSession : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::optional<VoiceBank> m_bank;
    std::unique_ptr<VoiceBankSession> m_session;

    template <class Edit>
    QList<QJsonObject> logOf(Edit edit) {
        QList<QJsonObject> entries;
        const auto connection =
            QObject::connect(m_session.get(), &edit::EditSession::changed, m_session.get(),
                             [&](const edit::ChangePtr &change) {
                                 if (const auto entry = m_session->logEntry(*change)) {
                                     entries.push_back(*entry);
                                 }
                             });
        auto transaction = m_session->transaction(QStringLiteral("Edit"));
        edit();
        transaction.commit();
        QObject::disconnect(connection);
        return entries;
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        auto opened = openRichBank(m_dir->path());
        QVERIFY(opened.has_value());
        m_bank = opened->bank;
        m_session = std::make_unique<VoiceBankSession>(std::move(*opened));
    }

    void cleanup() {
        m_session.reset();
        m_bank.reset();
        m_dir.reset();
    }

    // The snapshot is what saving and synthesis receive, including the samples without an entry,
    // which the tree does not hold.
    void the_snapshot_is_the_voice_bank_read() {
        QCOMPARE(m_session->rootPath(), m_bank->root());
        verifyEqual(m_session->snapshot(), *m_bank);
        verifyEqual(VoiceBankRef(m_session.get()).toVoiceBank(), *m_bank);
    }

    // A value is logged with the name of its field in VoiceBankSchema.h and its values.
    void a_value_is_logged_with_its_field_name_and_values() {
        const auto entry = VoiceBankRef(m_session.get()).directories().at(0).otoEntries().at(3);
        const auto entries = logOf([&] { entry.setAlias(QStringLiteral("n")); });
        QCOMPARE(entries, QList<QJsonObject>({
                              QJsonObject{{QStringLiteral("node"), qint64(entry.id())},
                                          {QStringLiteral("shape"), QStringLiteral("set")},
                                          {QStringLiteral("slot"), QStringLiteral("alias")},
                                          {QStringLiteral("before"), QStringLiteral("m")},
                                          {QStringLiteral("after"), QStringLiteral("n")}},
        }));
    }

    // A prefix is logged under its note number as the JSON a command writes.
    void a_prefix_is_logged_with_its_key_and_json() {
        const auto prefixMap = VoiceBankRef(m_session.get()).prefixMap();
        const auto entries = logOf(
            [&] { prefixMap.setValue(60, VoicePrefix{QStringLiteral("q"), QStringLiteral("s")}); });
        QCOMPARE(entries,
                 QList<QJsonObject>({
                     QJsonObject{{QStringLiteral("node"), qint64(prefixMap.id())},
                                 {QStringLiteral("shape"), QStringLiteral("entry")},
                                 {QStringLiteral("key"), QStringLiteral("60")},
                                 {QStringLiteral("before"),
                                  QJsonObject{{QStringLiteral("prefix"), QStringLiteral("p")},
                                              {QStringLiteral("suffix"), QStringLiteral("s")}}},
                                 {QStringLiteral("after"),
                                  QJsonObject{{QStringLiteral("prefix"), QStringLiteral("q")},
                                              {QStringLiteral("suffix"), QStringLiteral("s")}}}},
        }));
    }

    // A replaced character is logged as its JSON after the change.
    void a_replaced_character_is_logged_as_its_json() {
        const auto root = VoiceBankRef(m_session.get());
        VoiceCharacter character;
        character.name = QStringLiteral("n");
        const auto entries = logOf([&] { root.setCharacter(character); });
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().value(QStringLiteral("slot")),
                 QJsonValue(QStringLiteral("character")));
        QCOMPARE(
            entries.first().value(QStringLiteral("after")).toObject().value(QStringLiteral("name")),
            QJsonValue(QStringLiteral("n")));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankSession)

#include "test_VoiceBankSession.moc"
