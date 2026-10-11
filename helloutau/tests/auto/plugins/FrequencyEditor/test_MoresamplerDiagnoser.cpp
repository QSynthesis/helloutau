#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <FrequencyEditor/MoresamplerDiagnoser.h>

using namespace hello;
using namespace hello::daw;

namespace fs = std::filesystem;

class test_MoresamplerDiagnoser : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    fs::path resampler() const {
        return fs::path(m_dir.filePath(QStringLiteral("moresampler.exe")).toStdU16String());
    }

    void writeConfiguration(const QByteArray &content) {
        QFile file(m_dir.filePath(QStringLiteral("moreconfig.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(content);
    }

    // The record of a note whose fragment the wavtool rejected, with head as the start of the
    // fragment
    static kit::SynthFailure wavtoolFailure(const QByteArray &head) {
        kit::SynthFailure failure;
        failure.stage = kit::SynthFailure::Wavtool;
        failure.noteIndex = 2;
        failure.fileSize = 180;
        failure.fileHead = head;
        return failure;
    }

    kit::SynthDiagnosisContext contextOf(const QList<kit::SynthFailure> &failures,
                                         bool joinedInternally = false) const {
        kit::SynthDiagnosisContext context;
        context.synthTools = {resampler(), "C:/UTAU/wavtool.exe"};
        context.joinedInternally = joinedInternally;
        context.failures = failures;
        return context;
    }

    // The text that moresampler 0.8.4 writes with resampler-compatibility off, measured
    static QByteArray dummy() {
        return "This is a dummy file created just to fool UTAU's cache mechanism and you really "
               "shouldn't expect to see anything here.";
    }

private Q_SLOTS:
    void init() {
        QVERIFY(m_dir.isValid());
        QFile::remove(m_dir.filePath(QStringLiteral("moreconfig.txt")));
    }

    void the_option_is_read_beside_the_resampler() {
        QCOMPARE(MoresamplerDiagnoser::compatibility(resampler()), std::nullopt);
        writeConfiguration("# Output\r\noutput-bit-depth 16\r\nresampler-compatibility off\r\n");
        QCOMPARE(MoresamplerDiagnoser::compatibility(resampler()), std::optional<bool>(false));
        writeConfiguration("*resampler-compatibility   on  # forced\n");
        QCOMPARE(MoresamplerDiagnoser::compatibility(resampler()), std::optional<bool>(true));
        writeConfiguration("output-bit-depth 16\n");
        QCOMPARE(MoresamplerDiagnoser::compatibility(resampler()), std::nullopt);
    }

    void a_placeholder_fragment_is_diagnosed() {
        writeConfiguration("resampler-compatibility off\n");
        const MoresamplerDiagnoser diagnoser;
        const auto diagnoses =
            diagnoser.diagnose(contextOf({wavtoolFailure("RIFF"), wavtoolFailure(dummy())}));
        QCOMPARE(diagnoses.size(), 1);
        QCOMPARE(diagnoses.first().failures, QList<int>{1});
        QVERIFY(diagnoses.first().explanation.contains(QStringLiteral("moreconfig.txt")));
        // Set the option on, or the wavtool to moresampler, and clear the cache
        QCOMPARE(diagnoses.first().remedies.size(), 3);
    }

    // Realtime playback joins the fragments itself, so that another wavtool does not help.
    void realtime_playback_requires_the_option() {
        writeConfiguration("resampler-compatibility off\n");
        auto failure = wavtoolFailure(dummy());
        failure.stage = kit::SynthFailure::Fragment;
        const auto diagnoses = MoresamplerDiagnoser().diagnose(contextOf({failure}, true));
        QCOMPARE(diagnoses.size(), 1);
        QCOMPARE(diagnoses.first().remedies.size(), 2);
    }

    // In the classic mode, every existing fragment is recorded if the wavtool wrote no track,
    // and only the placeholders are claimed.
    void a_placeholder_of_the_classic_mode_is_diagnosed() {
        writeConfiguration("resampler-compatibility off\n");
        auto wave = wavtoolFailure("RIFF");
        wave.stage = kit::SynthFailure::Unconfirmed;
        auto placeholder = wavtoolFailure(dummy());
        placeholder.stage = kit::SynthFailure::Unconfirmed;
        const auto diagnoses = MoresamplerDiagnoser().diagnose(contextOf({wave, placeholder}));
        QCOMPARE(diagnoses.size(), 1);
        QCOMPARE(diagnoses.first().failures, QList<int>{1});
        QCOMPARE(diagnoses.first().remedies.size(), 3);
    }

    // With the option on, or without the file, moresampler writes wave files, so that the
    // fragments remain from a render before the change. Measured with moresampler 0.8.4.
    void with_the_option_on_the_cache_is_to_be_cleared() {
        writeConfiguration("resampler-compatibility on\n");
        const auto diagnoses =
            MoresamplerDiagnoser().diagnose(contextOf({wavtoolFailure(dummy())}));
        QCOMPARE(diagnoses.size(), 1);
        QCOMPARE(diagnoses.first().remedies.size(), 1);
    }

    void without_the_file_the_cache_is_to_be_cleared() {
        const auto diagnoses =
            MoresamplerDiagnoser().diagnose(contextOf({wavtoolFailure(dummy())}));
        QCOMPARE(diagnoses.size(), 1);
        QCOMPARE(diagnoses.first().remedies.size(), 1);
        QVERIFY(diagnoses.first().explanation.contains(QStringLiteral("moreconfig.txt")));
    }

    // The record of the track examines the track, which is not a fragment.
    void a_failure_of_the_track_is_not_diagnosed() {
        auto failure = wavtoolFailure(dummy());
        failure.noteIndex = std::nullopt;
        QVERIFY(MoresamplerDiagnoser().diagnose(contextOf({failure})).isEmpty());
    }
};

QTEST_GUILESS_MAIN(test_MoresamplerDiagnoser)

#include "test_MoresamplerDiagnoser.moc"
