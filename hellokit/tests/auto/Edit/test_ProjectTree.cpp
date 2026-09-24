#include <sstream>

#include <QtTest/QTest>

#include <substate/Codec.h>

#include "ProjectSamples.h"
#include "ProjectTree_p.h"

using namespace hello::kit;

class test_ProjectTree : public QObject {
    Q_OBJECT

private:
    static std::string encoded(const ss::Node *node) {
        std::stringstream buffer;
        ss::OBinaryStream out(buffer);
        ss::Encoder encoder(out);
        encoder.writeNode(node);
        return encoder.fail() ? std::string() : buffer.str();
    }

    static std::unique_ptr<ss::Node> decoded(const ss::QCodec &codec, const std::string &bytes) {
        std::stringstream buffer(bytes);
        ss::IBinaryStream in(buffer);
        ss::Decoder decoder(codec, in, nullptr);
        auto node = decoder.readNode();
        return decoder.fail() ? nullptr : std::move(node);
    }

    static void verifyCodecRoundTrip(const Project &project) {
        ss::QCodec codec;
        registerProjectTypes(codec);

        const auto tree = treeOf(project);
        const auto bytes = encoded(tree.get());
        QVERIFY(!bytes.empty());

        const auto back = decoded(codec, bytes);
        QVERIFY(back);
        QCOMPARE(fromTree<Project>(back.get()).toJson(), project.toJson());
    }

private Q_SLOTS:
    // A process that reads a history without having written these values finds their types by
    // name only after registration. This case runs first, before any encoding in this process
    // registers the types as a side effect.
    void the_value_types_are_found_by_name_after_registration() {
        ss::QCodec codec;
        registerProjectTypes(codec);
        QVERIFY(QMetaType::fromName("hello::kit::Envelope").isValid());
        QVERIFY(QMetaType::fromName("hello::kit::Vibrato").isValid());
    }

    // The edit history stores inserted subtrees in this encoding, including the envelope, the
    // vibrato and the unknown fields, which are stored as values of types outside the fixed
    // encoding of QVariant.
    void a_project_tree_survives_encoding() {
        verifyCodecRoundTrip(richProject());
    }

    void a_random_project_tree_survives_encoding() {
        for (quint32 seed = 1; seed <= 5; ++seed) {
            verifyCodecRoundTrip(randomProject(seed));
            if (QTest::currentTestFailed()) {
                QFAIL(qPrintable(QStringLiteral("seed %1").arg(seed)));
            }
        }
    }

    // The default types of the struct and array nodes do not determine their size or element
    // type, so decoding requires the registered node types.
    void decoding_requires_the_registered_node_types() {
        const auto tree = treeOf(richProject());
        const auto bytes = encoded(tree.get());
        QVERIFY(!bytes.empty());
        QVERIFY(!decoded(ss::QCodec(), bytes));
    }
};

QTEST_APPLESS_MAIN(test_ProjectTree)

#include "test_ProjectTree.moc"
