/// \file
/// **Most of what ThreadedSynthRunner does is not covered here yet.**
///
/// Running a note means starting the resampler and the wavtool, and a test has neither: the
/// engines are somebody else's programs and are not in this repository. What is left untested is
/// the part that only shows up with them present:
///
/// - that the wavtool's two pieces, \c <out>.whd and \c <out>.dat , are joined into the track
///   wav and then cleaned up
/// - that a second render clears the pieces a first one left, rather than appending to them
/// - that a note whose cache file did not appear is counted as failed and reported
/// - that \c stopOnFirstFailure stops
///
/// All four were checked by hand against UTAU's own engines. That is not a test.
///
/// Reuse is the exception and is covered: reusing a piece means not starting an engine, so a
/// test can point the engines at nothing and read the answer off the outcome.
///
/// **What it needs is a seam.** \c ThreadedSynthRunner reaches for \c EngineProcess directly, so
/// there is nowhere for a test to put an engine of its own. The same seam is what spreading the
/// resampler calls over threads needs, since a scheduler has to take those calls over, so the
/// two are one job and are waiting for it together.

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Synth/ThreadedSynthRunner.h>

using namespace hello::kit;

class test_ThreadedSynthRunner : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    std::filesystem::path root() const {
        return std::filesystem::path(m_dir->path().toStdU16String());
    }

    void write(const QString &relative, const QByteArray &bytes) {
        const QString path = m_dir->path() + QLatin1Char('/') + relative;
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    /// A plan for one note, which is as much as anything here needs.
    std::optional<SynthPlan> plan() {
        write(QStringLiteral("bank/oto.ini"), "a.wav=a,10,20,30,40,5\n");
        write(QStringLiteral("bank/a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root() / "bank", &selector, diagnostics);
        if (!bank) {
            return std::nullopt;
        }

        Note note;
        note.lyric = QStringLiteral("a");
        note.noteNum = 60;
        note.length = 480;

        Project project;
        Track track;
        track.notes.push_back(note);
        project.tracks.push_back(track);

        SynthPlan::Options options;
        options.cacheDirectory = root() / "cache";
        options.outputFile = root() / "out.wav";
        return SynthPlan::make(project, *bank, options, diagnostics);
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    // Reuse is the one part of rendering a test can see without an engine, because reusing is
    // exactly not starting one: the engines here point at nothing, so a note that comes back
    // reused can only have come back that way by being left alone.
    void a_piece_already_there_is_not_rendered_again() {
        const auto p = plan();
        QVERIFY(p.has_value());
        QCOMPARE(p->steps().size(), 1);

        QVERIFY(QDir().mkpath(QString::fromStdU16String(p->cacheDirectory().u16string())));
        write(QStringLiteral("cache/") +
                  QString::fromStdU16String(p->steps().at(0).cacheFile.filename().u16string()),
              "RIFF already rendered");

        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler.exe";
        engines.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        const auto outcome = runner.render(*p, engines, nullptr, diagnostics);

        QCOMPARE(outcome.reused, 1);
        QCOMPARE(outcome.resampled, 0);
        QCOMPARE(outcome.failed, 0);
    }

    // And the way out, for when the engine itself is what changed.
    void turning_reuse_off_renders_it_again() {
        const auto p = plan();
        QVERIFY(p.has_value());

        QVERIFY(QDir().mkpath(QString::fromStdU16String(p->cacheDirectory().u16string())));
        write(QStringLiteral("cache/") +
                  QString::fromStdU16String(p->steps().at(0).cacheFile.filename().u16string()),
              "RIFF already rendered");

        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler.exe";
        engines.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        ThreadedSynthRunner runner;
        runner.reuseCache = false;
        const auto outcome = runner.render(*p, engines, nullptr, diagnostics);

        QCOMPARE(outcome.reused, 0);
    }

    // The folder is the project's and lives as long as it does, so the pieces a note rendered to
    // before it was edited have to go. What belongs to notes this render is not touching stays.
    void the_pieces_a_note_no_longer_wants_are_cleared() {
        const auto p = plan();
        QVERIFY(p.has_value());

        const QString cache = QStringLiteral("cache/");
        const QString wanted =
            QString::fromStdU16String(p->steps().at(0).cacheFile.filename().u16string());
        write(cache + QStringLiteral("0_a_C4_gone00.wav"), "an older take of this note");
        write(cache + QStringLiteral("7_a_C4_stays0.wav"), "a note this render is not touching");
        write(cache + QStringLiteral("notes.txt"), "not a piece at all");
        write(cache + wanted, "RIFF already rendered");

        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler.exe";
        engines.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        runner.render(*p, engines, nullptr, diagnostics);

        const auto there = [&](const QString &name) {
            return std::filesystem::exists(p->cacheDirectory() /
                                           std::filesystem::u8path(name.toStdString()));
        };
        QVERIFY(!there(QStringLiteral("0_a_C4_gone00.wav")));
        QVERIFY(there(QStringLiteral("7_a_C4_stays0.wav")));
        QVERIFY(there(QStringLiteral("notes.txt")));
        QVERIFY(there(wanted));
    }

    // An engine that is not where it was said to be has to be said out loud. Rendering nothing
    // and reporting nothing is the failure that would waste a user's afternoon.
    void engines_that_are_not_there_are_reported() {
        const auto p = plan();
        QVERIFY(p.has_value());

        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler.exe";
        engines.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        const auto outcome = runner.render(*p, engines, nullptr, diagnostics);

        QVERIFY(!outcome.rendered);
        QCOMPARE(outcome.resampled, 0);
        QCOMPARE(outcome.failed, 1);
        QVERIFY(hasError(diagnostics));
        QVERIFY(!std::filesystem::exists(p->outputFile()));
    }

    // The engines write into it, so it has to be there before they run. They are not going to
    // create it, and the diagnostic they give for a path that does not exist is their own.
    void the_cache_folder_is_created() {
        const auto p = plan();
        QVERIFY(p.has_value());
        QVERIFY(!std::filesystem::exists(p->cacheDirectory()));

        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler.exe";
        engines.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const ThreadedSynthRunner runner;
        runner.render(*p, engines, nullptr, diagnostics);

        QVERIFY(std::filesystem::is_directory(p->cacheDirectory()));
    }
};

QTEST_APPLESS_MAIN(test_ThreadedSynthRunner)

#include "test_ThreadedSynthRunner.moc"
