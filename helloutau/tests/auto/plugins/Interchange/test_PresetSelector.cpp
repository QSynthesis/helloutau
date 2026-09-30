#include <QtTest/QTest>

#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeWriter.h>

#include <Interchange/PresetSelector.h>

using namespace hello;
using namespace hello::daw;

namespace {

    // A driver whose inspection returns the given entry indices. convert() records the request
    // that read() passes to it.
    class FixedReader : public kit::InterchangeReader {
    public:
        explicit FixedReader(QList<int> indices) : m_indices(std::move(indices)) {
        }

        QString id() const override {
            return QStringLiteral("fixed");
        }
        QString name() const override {
            return id();
        }
        QStringList suffixes() const override {
            return {id()};
        }

        std::optional<kit::InterchangeSource> inspect(const std::filesystem::path &,
                                                      kit::DiagnosticList &) override {
            kit::InterchangeSource source;
            for (const int index : std::as_const(m_indices)) {
                kit::InterchangeEntry entry;
                entry.index = index;
                source.entries.push_back(entry);
            }
            return source;
        }

        // The request passed to convert(), if it was called
        std::optional<kit::ImportRequest> received;

    protected:
        std::optional<kit::Project> convert(const std::filesystem::path &,
                                            const kit::InterchangeSource &,
                                            const kit::ImportRequest &request,
                                            kit::DiagnosticList &) override {
            received = request;
            return kit::Project();
        }

    private:
        QList<int> m_indices;
    };

    kit::ImportRequest requestOf(QList<int> entries) {
        kit::ImportRequest request;
        request.entries = std::move(entries);
        request.driverOptions.insert(QStringLiteral("encoding"), QStringLiteral("Shift_JIS"));
        return request;
    }

}

class test_PresetSelector : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // read() passes the prepared request to the driver.
    void the_import_takes_the_answers() {
        FixedReader reader({0, 1});
        PresetSelector selector(requestOf({1}), 2);
        const auto result = reader.read("a.fixed", &selector);
        QVERIFY(result.project);
        QVERIFY(reader.received);
        QCOMPARE(reader.received->entries, QList<int>({1}));
        QCOMPARE(reader.received->driverOptions.value(QStringLiteral("encoding")),
                 QVariant(QStringLiteral("Shift_JIS")));
    }

    // An entry count or entry index that differs from the prepared request is an error.
    void a_changed_file_fails() {
        {
            FixedReader reader({0, 1, 2});
            PresetSelector selector(requestOf({1}), 2);
            const auto result = reader.read("a.fixed", &selector);
            QVERIFY(!result.project);
            QVERIFY(!result.cancelled);
            QVERIFY(kit::hasError(result.diagnostics));
            QVERIFY(!reader.received);
        }
        {
            FixedReader reader({0, 2});
            PresetSelector selector(requestOf({1}), 2);
            const auto result = reader.read("a.fixed", &selector);
            QVERIFY(!result.project);
            QVERIFY(kit::hasError(result.diagnostics));
        }
    }

    // A request with more entries than ImportLimits::maxEntries is an error.
    void the_limits_hold() {
        FixedReader reader({0, 1});
        PresetSelector selector(requestOf({0, 1}), 2);
        const auto result = reader.read("a.fixed", &selector);
        QVERIFY(!result.project);
        QVERIFY(kit::hasError(result.diagnostics));
    }

    void the_export_takes_the_answers() {
        kit::ExportRequest request;
        request.driverOptions.insert(QStringLiteral("encoding"), QStringLiteral("GBK"));
        PresetSelector selector(request);
        kit::DiagnosticList diagnostics;
        class : public kit::InterchangeWriter {
        public:
            QString id() const override {
                return {};
            }
            QString name() const override {
                return {};
            }
            QStringList suffixes() const override {
                return {};
            }

        protected:
            bool convert(const kit::Project &, const std::filesystem::path &,
                         const kit::ExportRequest &, kit::DiagnosticList &) override {
                return false;
            }
        } writer;
        const auto answer = selector.selectExport(writer, kit::Project(), diagnostics);
        QVERIFY(answer);
        QCOMPARE(answer->driverOptions, request.driverOptions);
        // selectImport() of a selector prepared for an export is an error.
        QVERIFY(
            !selector.selectImport(FixedReader({0}), kit::InterchangeSource(), {}, diagnostics));
        QVERIFY(kit::hasError(diagnostics));
    }
};

QTEST_APPLESS_MAIN(test_PresetSelector)

#include "test_PresetSelector.moc"
