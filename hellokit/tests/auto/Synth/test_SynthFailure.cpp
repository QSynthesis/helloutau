#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Synth/SynthFailure.h>

using namespace hello::kit;

class test_SynthFailure : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_wave_file_is_recognized_by_its_header() {
        QVERIFY(SynthFailure::isWave(QByteArray("RIFF\x24\xd6\x00\x00WAVEfmt ", 16)));
        QVERIFY(!SynthFailure::isWave(QByteArray("RIFF\x24\xd6\x00\x00"
                                                 "AVI LIST",
                                                 16)));
        QVERIFY(!SynthFailure::isWave("This is a dummy file created just to fool UTAU's"));
        QVERIFY(!SynthFailure::isWave("RIFF"));
        QVERIFY(!SynthFailure::isWave({}));
    }

    void the_output_is_read_up_to_the_head_size() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SynthFailure failure;
        failure.file = directory.filePath(QStringLiteral("out.wav")).toStdU16String();
        failure.readFile();
        QCOMPARE(failure.fileSize, -1);
        QVERIFY(failure.fileHead.isEmpty());

        QFile file(directory.filePath(QStringLiteral("out.wav")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray content(SynthFailure::headSize + 100, 'x');
        file.write(content);
        file.close();
        failure.readFile();
        QCOMPARE(failure.fileSize, content.size());
        QCOMPARE(failure.fileHead, content.first(SynthFailure::headSize));

        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("short");
        file.close();
        failure.readFile();
        QCOMPARE(failure.fileSize, 5);
        QCOMPARE(failure.fileHead, QByteArray("short"));
    }
};

QTEST_APPLESS_MAIN(test_SynthFailure)

#include "test_SynthFailure.moc"
