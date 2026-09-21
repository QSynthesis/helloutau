/// \file
/// **Most of what SynthRunner does is not covered here yet.**
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
/// **What it needs is a seam.** \c SynthRunner reaches for \c EngineProcess directly, so there
/// is nowhere for a test to put an engine of its own. The same seam is what spreading the
/// resampler calls over threads needs, since a scheduler has to take those calls over, so the
/// two are one job and are waiting for it together.

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Synth/SynthRunner.h>

using namespace hello::kit;

class test_SynthRunner : public QObject {
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

    // An engine that is not where it was said to be has to be said out loud. Rendering nothing
    // and reporting nothing is the failure that would waste a user's afternoon.
    void engines_that_are_not_there_are_reported() {
        const auto p = plan();
        QVERIFY(p.has_value());

        SynthEngines engines;
        engines.resampler = root() / "nowhere" / "resampler.exe";
        engines.wavtool = root() / "nowhere" / "wavtool.exe";

        DiagnosticList diagnostics;
        const SynthRunner runner;
        const auto outcome = runner.render(*p, engines, diagnostics);

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
        const SynthRunner runner;
        runner.render(*p, engines, diagnostics);

        QVERIFY(std::filesystem::is_directory(p->cacheDirectory()));
    }
};

QTEST_APPLESS_MAIN(test_SynthRunner)

#include "test_SynthRunner.moc"
