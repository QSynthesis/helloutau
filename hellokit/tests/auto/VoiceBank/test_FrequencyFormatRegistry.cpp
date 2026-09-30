#include <filesystem>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QtEndian>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/FrequencyFormatPlugin.h>
#include <hellokit/VoiceBank/FrequencyFormatRegistry.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    template <class T>
    QByteArray bytesOf(T value) {
        QByteArray bytes(sizeof(T), '\0');
        qToLittleEndian(value, bytes.data());
        return bytes;
    }

    QByteArray utf16(const QString &text) {
        QByteArray bytes;
        for (const auto c : text) {
            bytes += bytesOf<quint16>(c.unicode());
        }
        return bytes;
    }

    void write(const fs::path &path, const QByteArray &bytes) {
        QFile file(QString::fromStdU16String(path.u16string()));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
    }

    // An entry of desc.mrq of version 2
    QByteArray mrqEntry(const QString &name, const QList<float> &frequencies) {
        QByteArray data = bytesOf<qint32>(qint32(frequencies.size())) + bytesOf<qint32>(44100) +
                          bytesOf<qint32>(256);
        for (const float f : frequencies) {
            data += bytesOf(f);
        }
        data += bytesOf<qint32>(0) + bytesOf<qint32>(1);
        return bytesOf<qint32>(qint32(name.size())) + utf16(name) +
               bytesOf<qint32>(qint32(data.size())) + data;
    }

    class Fixed : public FrequencyFormat {
    public:
        explicit Fixed(QString id, QStringList patterns = {})
            : m_id(std::move(id)), m_patterns(std::move(patterns)) {
        }

        QString id() const override {
            return m_id;
        }

        QString name() const override {
            return m_id;
        }

        QStringList resamplerPatterns() const override {
            return m_patterns;
        }

        bool exists(const fs::path &) const override {
            return false;
        }

        std::optional<FrequencyTable> read(const fs::path &, int, DiagnosticList &) const override {
            return std::nullopt;
        }

    private:
        QString m_id;
        QStringList m_patterns;
    };

    class Plugin : public FrequencyFormatPlugin {
    public:
        std::vector<std::unique_ptr<FrequencyFormat>> createFormats() override {
            std::vector<std::unique_ptr<FrequencyFormat>> formats;
            formats.push_back(std::make_unique<Fixed>(QStringLiteral("sc"),
                                                      QStringList{QStringLiteral("straycat*")}));
            formats.push_back(std::make_unique<Fixed>(QStringLiteral("frq")));
            return formats;
        }
    };

}

class test_FrequencyFormatRegistry : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    fs::path pathOf(const char *name) const {
        return fs::path(m_dir.path().toStdU16String()) / name;
    }

private Q_SLOTS:
    // The format of a resampler is chosen by its file name alone; frq otherwise. A plugin
    // format joins, and cannot take over an ID that is registered.
    void a_format_is_chosen_by_the_resampler() {
        FrequencyFormatRegistry registry;
        QVERIFY(!registry.formatForResampler("resampler.exe"));
        registry.addBuiltinFormats();
        QCOMPARE(registry.formats().size(), 3);
        const auto idOf = [&registry](const char *resampler) {
            const auto format = registry.formatForResampler(fs::path(resampler));
            return format ? format->id() : QString();
        };
        QCOMPARE(idOf("C:/UTAU/resampler.exe"), QStringLiteral("frq"));
        QCOMPARE(idOf("MoreSampler.EXE"), QStringLiteral("mrq"));
        QCOMPARE(idOf("moresampler-0.8.4.exe"), QStringLiteral("mrq"));
        QCOMPARE(idOf("w4u.exe"), QStringLiteral("dio"));
        QCOMPARE(idOf("world4utau.exe"), QStringLiteral("dio"));
        QCOMPARE(idOf("fresamp14.exe"), QStringLiteral("frq"));
        QCOMPARE(idOf(""), QStringLiteral("frq"));

        Plugin plugin;
        int added = 0;
        for (auto &format : plugin.createFormats()) {
            added += registry.add(std::move(format)) ? 1 : 0;
        }
        QCOMPARE(added, 1);
        QCOMPARE(idOf("straycat.exe"), QStringLiteral("sc"));
        QCOMPARE(registry.format(QStringLiteral("frq"))->name(),
                 QStringLiteral("frq (resampler.exe)"));
    }

    // a_wav.frq: frames every hop samples of the rate of the audio file
    void a_frq_reads() {
        FrequencyFormatRegistry registry;
        registry.addBuiltinFormats();
        const auto frq = registry.format(QStringLiteral("frq"));
        const auto wav = pathOf("a.wav");
        QVERIFY(!frq->exists(wav));
        DiagnosticList diagnostics;
        QVERIFY(!frq->read(wav, 44100, diagnostics));
        QCOMPARE(diagnostics.size(), 1);

        QByteArray bytes("FREQ0003");
        bytes += bytesOf<qint32>(256) + bytesOf(220.5) + QByteArray(16, 'x') + bytesOf<qint32>(3);
        for (const double f : {0.0, 220.0, 221.0}) {
            bytes += bytesOf(f) + bytesOf(f / 10);
        }
        write(pathOf("a_wav.frq"), bytes);
        QVERIFY(frq->exists(wav));
        diagnostics.clear();
        const auto table = frq->read(wav, 44100, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->averageFrequency, std::optional<double>(220.5));
        QCOMPARE(table->frames.size(), size_t(3));
        QCOMPARE(table->frames[2].time, 2 * 256 * 1000 / 44100.0);
        QCOMPARE(table->frames[1].frequency, 220.0);
        QCOMPARE(table->frames[2].amplitude, std::optional<double>(22.1));
        QVERIFY(diagnostics.isEmpty());

        // Cut short, or of another kind
        write(pathOf("a_wav.frq"), bytes.left(bytes.size() - 1));
        QVERIFY(!frq->read(wav, 44100, diagnostics));
        write(pathOf("a_wav.frq"), "FREQ0002");
        QVERIFY(!frq->read(wav, 44100, diagnostics));
        QCOMPARE(diagnostics.size(), 2);
    }

    // b.dio: the time of each frame as written, in seconds
    void a_dio_reads() {
        FrequencyFormatRegistry registry;
        registry.addBuiltinFormats();
        const auto dio = registry.format(QStringLiteral("dio"));
        QByteArray bytes("wrld-dio");
        bytes += bytesOf<qint32>(1000) + bytesOf<qint32>(44100) + bytesOf<qint32>(2);
        bytes += bytesOf(0.0) + bytesOf(0.0) + bytesOf(0.005) + bytesOf(261.6);
        write(pathOf("b.dio"), bytes);
        const auto wav = pathOf("b.wav");
        QVERIFY(dio->exists(wav));
        DiagnosticList diagnostics;
        const auto table = dio->read(wav, 44100, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->frames.size(), size_t(2));
        QCOMPARE(table->frames[1].time, 5.0);
        QCOMPARE(table->frames[1].frequency, 261.6);
        QVERIFY(!table->frames[1].amplitude);
        QVERIFY(!table->averageFrequency);
    }

    // An entry of desc.mrq, found by name past a deleted entry, its case aside
    void an_mrq_entry_reads() {
        QDir(m_dir.path()).mkdir(QStringLiteral("m"));
        FrequencyFormatRegistry registry;
        registry.addBuiltinFormats();
        const auto mrq = registry.format(QStringLiteral("mrq"));
        const auto wav = pathOf("m") / fs::path(u"い.wav");
        QVERIFY(!mrq->exists(wav));
        QByteArray bytes("mrq ");
        bytes += bytesOf<qint32>(2) + bytesOf<qint32>(3);
        bytes += mrqEntry(QString(5, QChar(0)), {1.0f});
        bytes += mrqEntry(QStringLiteral("x.wav"), {2.0f});
        bytes += mrqEntry(QStringLiteral("い.WAV"), {0.0f, 440.0f});
        write(pathOf("m") / "desc.mrq", bytes);
        QVERIFY(mrq->exists(wav));
        QVERIFY(!mrq->exists(pathOf("m") / "y.wav"));
        DiagnosticList diagnostics;
        const auto table = mrq->read(wav, 0, diagnostics);
        QVERIFY(table);
        QCOMPARE(table->frames.size(), size_t(2));
        QCOMPARE(table->frames[1].frequency, 440.0);
        QCOMPARE(table->frames[1].time, 256 * 1000 / 44100.0);
        QVERIFY(!mrq->read(pathOf("m") / "y.wav", 0, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
    }
};

QTEST_GUILESS_MAIN(test_FrequencyFormatRegistry)

#include "test_FrequencyFormatRegistry.moc"
