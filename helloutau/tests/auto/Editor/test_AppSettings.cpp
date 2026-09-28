#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <helloutau/Editor/AppSettings.h>

using namespace hello::daw;

class test_AppSettings : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void unset_values_are_empty_except_the_export_encoding() {
        QTemporaryDir dir;
        const AppSettings settings(dir.filePath(QStringLiteral("settings.ini")));
        QVERIFY(settings.utauDirectory().empty());
        QVERIFY(settings.resampler().isEmpty());
        QVERIFY(settings.wavtool().isEmpty());
        QCOMPARE(settings.ustExportCharset(), QStringLiteral("UTF-8"));
    }

    void values_persist_in_the_file() {
        QTemporaryDir dir;
        const auto file = dir.filePath(QStringLiteral("settings.ini"));
        const auto utau = std::filesystem::path(u"C:/UTAU/歌");
        {
            AppSettings settings(file);
            settings.setUtauDirectory(utau);
            settings.setResampler(QStringLiteral("C:/UTAU/resampler.exe"));
            settings.setWavtool(QStringLiteral("C:/UTAU/wavtool.exe"));
            settings.setUstExportCharset(QStringLiteral("Shift_JIS"));
        }
        const AppSettings settings(file);
        QCOMPARE(settings.utauDirectory(), utau);
        QCOMPARE(settings.resampler(), QStringLiteral("C:/UTAU/resampler.exe"));
        QCOMPARE(settings.wavtool(), QStringLiteral("C:/UTAU/wavtool.exe"));
        QCOMPARE(settings.ustExportCharset(), QStringLiteral("Shift_JIS"));
    }
};

QTEST_APPLESS_MAIN(test_AppSettings)

#include "test_AppSettings.moc"
