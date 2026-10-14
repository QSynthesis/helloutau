#include <QtTest/QTest>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>

#include <hellokit/VoiceBank/FrequencyFormat.h>

#include <helloutau/Editor/Dialogs/ConvertFrequencyMapsDialog.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    // A format with an identifier, which writes maps if writable is set
    class Format : public kit::FrequencyFormat {
    public:
        Format(QString id, bool writable) : m_id(std::move(id)), m_writable(writable) {
        }

        QString id() const override {
            return m_id;
        }
        QString name() const override {
            return m_id;
        }
        QStringList resamplerPatterns() const override {
            return {};
        }
        QList<bool> mapsExist(const fs::path &, const QList<fs::path> &wavs) const override {
            return QList<bool>(wavs.size(), false);
        }
        std::optional<kit::FrequencyMap> read(const fs::path &, int,
                                              kit::DiagnosticList &) const override {
            return std::nullopt;
        }
        bool canWrite() const override {
            return m_writable;
        }

    private:
        QString m_id;
        bool m_writable;
    };

    QStringList itemsOf(const QComboBox *box) {
        QStringList items;
        for (int i = 0; i < box->count(); ++i) {
            items.push_back(box->itemText(i));
        }
        return items;
    }

}

class test_ConvertFrequencyMapsDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Every format is a source format, and only the formats that write maps are target formats.
    // The initial target format is the first writable format other than the source format.
    void the_target_formats_write_maps() {
        Format a(QStringLiteral("a"), true);
        Format b(QStringLiteral("b"), false);
        Format c(QStringLiteral("c"), true);
        const ConvertFrequencyMapsDialog dialog({&a, &b, &c}, &a, 3);
        QCOMPARE(itemsOf(dialog.sourceBox()), (QStringList{"a", "b", "c"}));
        QCOMPARE(itemsOf(dialog.targetBox()), (QStringList{"a", "c"}));
        QCOMPARE(dialog.sourceFormat(), &a);
        QCOMPARE(dialog.targetFormat(), &c);
    }

    // The conversion requires two different formats.
    void the_formats_must_differ() {
        Format a(QStringLiteral("a"), true);
        Format b(QStringLiteral("b"), true);
        const ConvertFrequencyMapsDialog dialog({&a, &b}, &b, 1);
        const auto ok = dialog.buttonBox()->button(QDialogButtonBox::Ok);
        QCOMPARE(dialog.targetFormat(), &a);
        QVERIFY(ok->isEnabled());
        dialog.targetBox()->setCurrentIndex(1);
        QCOMPARE(dialog.targetFormat(), &b);
        QVERIFY(!ok->isEnabled());
    }

    // Without a writable format there is no target format.
    void no_target_without_a_writable_format() {
        Format a(QStringLiteral("a"), false);
        const ConvertFrequencyMapsDialog dialog({&a}, &a, 1);
        QCOMPARE(dialog.targetFormat(), nullptr);
        QVERIFY(!dialog.buttonBox()->button(QDialogButtonBox::Ok)->isEnabled());
    }
};

QTEST_MAIN(test_ConvertFrequencyMapsDialog)

#include "test_ConvertFrequencyMapsDialog.moc"
