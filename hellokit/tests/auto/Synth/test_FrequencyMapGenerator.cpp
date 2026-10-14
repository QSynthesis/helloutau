#include <atomic>
#include <fstream>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Synth/FrequencyMapGenerator.h>
#include <hellokit/Synth/SynthToolProcess.h>

using namespace hello::kit;

namespace fs = std::filesystem;

namespace {

    // A format whose map of a.wav is a.map beside it
    class SidecarFormat : public FrequencyFormat {
    public:
        QString id() const override {
            return QStringLiteral("map");
        }
        QString name() const override {
            return QStringLiteral("map");
        }
        QStringList resamplerPatterns() const override {
            return {};
        }
        QList<bool> mapsExist(const fs::path &directory,
                              const QList<fs::path> &wavs) const override {
            QList<bool> found;
            for (const auto &wav : wavs) {
                std::error_code error;
                found.push_back(fs::is_regular_file(directory / *sidecarNameOf(wav), error));
            }
            return found;
        }

        bool hasMap(const fs::path &wav) const {
            return mapsExist(wav.parent_path(), {wav.filename()}).value(0);
        }
        std::optional<FrequencyMap> read(const fs::path &, int, DiagnosticList &) const override {
            return std::nullopt;
        }
        std::optional<fs::path> sidecarNameOf(const fs::path &wav) const override {
            return fs::path(wav.stem()) += ".map";
        }
    };

    // A resampler that writes the map of its input unless the name of the input contains "silent"
    // or "fails", the latter with the exit code 1, and records the arguments of each call
    class StandIn : public SynthToolProcess {
    public:
        bool starts = true;
        mutable QMutex mutex;
        mutable QList<QStringList> calls;

        SynthToolRun run(const fs::path &, const QStringList &arguments, DiagnosticList &,
                         const std::function<bool()> &canceled) const override {
            {
                const QMutexLocker locker(&mutex);
                calls.push_back(arguments);
            }
            SynthToolRun out;
            out.started = starts;
            if (!starts) {
                return out;
            }
            if (canceled && canceled()) {
                out.canceled = true;
                return out;
            }
            const fs::path wav(arguments.at(0).toStdU16String());
            if (arguments.at(0).contains(QStringLiteral("fails"))) {
                out.exitCode = 1;
            } else if (!arguments.at(0).contains(QStringLiteral("silent"))) {
                std::ofstream(wav.parent_path() / (fs::path(wav.stem()) += ".map")) << "map";
            }
            std::ofstream(fs::path(arguments.at(1).toStdU16String())) << "RIFF";
            out.output = QStringLiteral("rendered");
            return out;
        }
    };

    // A format whose maps cannot be read until the stand-in has received the number of calls
    // passed to the constructor, because desc.mrq cannot be read while another moresampler locks
    // it to write an entry
    class LockedFormat : public SidecarFormat {
    public:
        LockedFormat(const StandIn &standIn, int total) : m_standIn(standIn), m_total(total) {
        }

        QList<bool> mapsExist(const fs::path &directory,
                              const QList<fs::path> &wavs) const override {
            const QMutexLocker locker(&m_standIn.mutex);
            if (m_standIn.calls.size() < m_total) {
                return QList<bool>(wavs.size(), false);
            }
            return SidecarFormat::mapsExist(directory, wavs);
        }

    private:
        const StandIn &m_standIn;
        const int m_total;
    };

}

class test_FrequencyMapGenerator : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    fs::path pathOf(const char *name) const {
        return fs::path(m_dir->path().toStdU16String()) / name;
    }

    fs::path write(const char *name) const {
        const auto path = pathOf(name);
        std::ofstream(path) << "RIFF";
        return path;
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    // Each audio file without a map is rendered as a short note, whose output is removed, and an
    // audio file with a map is skipped. Every audio file counts for the progress.
    void the_missing_maps_are_generated() {
        const QList<fs::path> wavs{write("a.wav"), write("b.wav"), write("c.wav")};
        write("b.map");
        const SidecarFormat format;
        FrequencyMapGenerator generator;
        const auto standIn = std::make_shared<StandIn>();
        generator.process = standIn;
        std::atomic<int> last = 0;
        DiagnosticList diagnostics;
        const auto outcome =
            generator.generate(format, "resampler.exe", wavs, diagnostics, [&last](int done) {
                int seen = last.load();
                while (done > seen && !last.compare_exchange_weak(seen, done)) {
                }
            });
        QCOMPARE(outcome.generated, 2);
        QVERIFY(outcome.failures.isEmpty());
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(last.load(), 3);
        QVERIFY(format.hasMap(wavs.at(0)));
        QVERIFY(format.hasMap(wavs.at(2)));
        QCOMPARE(standIn->calls.size(), 2);
        for (const auto &arguments : std::as_const(standIn->calls)) {
            QVERIFY(arguments.at(0).endsWith(QStringLiteral(".wav")));
            QVERIFY(!arguments.at(0).contains(QStringLiteral("b.wav")));
            QCOMPARE(arguments.at(2), QStringLiteral("C4"));
            QVERIFY(!fs::exists(fs::path(arguments.at(1).toStdU16String())));
        }
    }

    // With everyFile, an audio file with a map is rendered as well, for its caches.
    void every_file_is_rendered_if_asked() {
        const QList<fs::path> wavs{write("a.wav"), write("b.wav")};
        write("b.map");
        const SidecarFormat format;
        FrequencyMapGenerator generator;
        generator.everyFile = true;
        const auto standIn = std::make_shared<StandIn>();
        generator.process = standIn;
        DiagnosticList diagnostics;
        QCOMPARE(generator.generate(format, "resampler.exe", wavs, diagnostics).generated, 2);
        QCOMPARE(standIn->calls.size(), 2);
        QVERIFY(diagnostics.isEmpty());
    }

    // An audio file whose map the resampler does not write is reported, and its call is recorded
    // for the diagnosis with the output, which the diagnostic does not include. A call that ends
    // with an exit code other than 0 failed in the resampler, and a call that ends normally
    // failed to write the map.
    void a_map_not_written_is_reported() {
        const QList<fs::path> wavs{write("silent.wav"), write("a.wav"), write("fails.wav")};
        const SidecarFormat format;
        FrequencyMapGenerator generator;
        generator.threadCount = 1;
        generator.process = std::make_shared<StandIn>();
        DiagnosticList diagnostics;
        const auto outcome = generator.generate(format, "resampler.exe", wavs, diagnostics);
        QCOMPARE(outcome.generated, 1);
        QCOMPARE(diagnostics.size(), 2);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);
        QVERIFY(diagnostics.at(0).message.contains(QStringLiteral("silent.wav")));
        QVERIFY(!diagnostics.at(0).message.contains(QStringLiteral("rendered")));

        QCOMPARE(outcome.failures.size(), 2);
        const auto &silent = outcome.failures.at(0);
        QCOMPARE(silent.stage, SynthFailure::FrequencyMap);
        QCOMPARE(silent.program, fs::path("resampler.exe"));
        QVERIFY(silent.arguments.at(0).endsWith(QStringLiteral("silent.wav")));
        QCOMPARE(silent.run->output, QStringLiteral("rendered"));
        QCOMPARE(silent.fileHead, QByteArray("RIFF"));
        QCOMPARE(outcome.failures.at(1).stage, SynthFailure::Resampler);
        QCOMPARE(outcome.failures.at(1).run->exitCode, 1);
    }

    // The maps are checked once every resampler call has finished, so that a map file that cannot
    // be read while a later call writes it is not reported as missing.
    void the_maps_are_checked_after_every_call() {
        const QList<fs::path> wavs{write("a.wav"), write("b.wav")};
        const auto standIn = std::make_shared<StandIn>();
        const LockedFormat format(*standIn, 2);
        FrequencyMapGenerator generator;
        generator.threadCount = 1;
        generator.process = standIn;
        DiagnosticList diagnostics;
        const auto outcome = generator.generate(format, "resampler.exe", wavs, diagnostics);
        QCOMPARE(outcome.generated, 2);
        QVERIFY(outcome.failures.isEmpty());
        QVERIFY(diagnostics.isEmpty());
    }

    // A resampler that does not start is reported once, and no further audio file is started.
    void a_resampler_that_does_not_start_stops_the_generation() {
        const QList<fs::path> wavs{write("a.wav"), write("b.wav"), write("c.wav")};
        const SidecarFormat format;
        FrequencyMapGenerator generator;
        generator.threadCount = 1;
        const auto standIn = std::make_shared<StandIn>();
        standIn->starts = false;
        generator.process = standIn;
        DiagnosticList diagnostics;
        const auto outcome = generator.generate(format, "resampler.exe", wavs, diagnostics);
        QCOMPARE(outcome.generated, 0);
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Error);
        QCOMPARE(standIn->calls.size(), 1);
        QCOMPARE(outcome.failures.size(), 1);
        QVERIFY(!outcome.failures.at(0).run->started);
    }

    // Once canceled, no further audio file is started.
    void a_canceled_generation_starts_nothing_more() {
        const QList<fs::path> wavs{write("a.wav"), write("b.wav"), write("c.wav")};
        const SidecarFormat format;
        FrequencyMapGenerator generator;
        generator.threadCount = 1;
        const auto standIn = std::make_shared<StandIn>();
        generator.process = standIn;
        std::atomic<int> done = 0;
        DiagnosticList diagnostics;
        QCOMPARE(generator
                     .generate(
                         format, "resampler.exe", wavs, diagnostics,
                         [&done](int count) { done = count; }, [&done] { return done.load() >= 1; })
                     .generated,
                 1);
        QVERIFY(diagnostics.isEmpty());
        QCOMPARE(standIn->calls.size(), 1);
        QCOMPARE(done.load(), 3);
    }
};

QTEST_GUILESS_MAIN(test_FrequencyMapGenerator)
#include "test_FrequencyMapGenerator.moc"
