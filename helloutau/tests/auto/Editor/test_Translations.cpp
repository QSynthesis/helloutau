#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>
#include <QtTest/QTest>

#include <helloutau/Editor/Translations.h>

using namespace hello::daw;

class test_Translations : public QObject {
    Q_OBJECT

    static QString newText() {
        return QCoreApplication::translate("hello::daw::ActionText", "&New");
    }

private Q_SLOTS:
    // The settings offer the system language, English and Simplified Chinese, the last two
    // named in themselves.
    void the_languages_are_offered_in_themselves() {
        const auto languages = Translations::languages();
        QCOMPARE(languages.size(), 3);
        QVERIFY(languages[0].first.isEmpty());
        QCOMPARE(languages[1], (std::pair{QStringLiteral("en"), QStringLiteral("English")}));
        QCOMPARE(languages[2], (std::pair{QStringLiteral("zh_CN"), QStringLiteral("简体中文")}));
        QCOMPARE(Translations::localeOf(QString()), QLocale::system());
        QCOMPARE(Translations::localeOf(QStringLiteral("zh_CN")).language(), QLocale::Chinese);
    }

    // For English nothing is installed; for Simplified Chinese the translations embedded in
    // the library translate the menus, and those of a plugin are not found here.
    void the_translations_follow_the_language() {
        Translations::install(QStringLiteral("en"));
        QCOMPARE(QLocale().language(), QLocale::English);
        QCOMPARE(newText(), QStringLiteral("&New"));

        Translations::install(QStringLiteral("zh_CN"));
        QCOMPARE(QLocale().language(), QLocale::Chinese);
        QCOMPARE(newText(), QStringLiteral("新建(&N)"));
        QCOMPARE(QCoreApplication::translate("hello::daw::AudioSettingPage", "Audio"),
                 QStringLiteral("音频"));
        QCOMPARE(QCoreApplication::translate("hello::daw::AudioSettingPage", "System default"),
                 QStringLiteral("系统默认"));
        QCOMPARE(QCoreApplication::translate("hello::daw::ProjectPropertiesDialog",
                                             "The project wavtool is trusted."),
                 QStringLiteral("工程中的合成器已信任。"));
        QCOMPARE(QCoreApplication::translate("hello::daw::ProjectPropertiesDialog",
                                             "The project resampler is trusted."),
                 QStringLiteral("工程中的重采样器已信任。"));
        QVERIFY(!Translations::load(QStringLiteral("Core"),
                                    QStringLiteral(":/helloutau/plugins/Core/translations")));
    }
};

QTEST_GUILESS_MAIN(test_Translations)

#include "test_Translations.moc"
