#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QLocale>
#include <QtTest/QTest>

#include <hellokit/Support/TranslationLoader.h>

using namespace hello::kit;

namespace {

    // A text that the translation of Qt for Simplified Chinese translates
    QString okText() {
        return QCoreApplication::translate("QErrorMessage", "&OK");
    }

    QString qtTranslations() {
        return QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    }

}

class test_TranslationLoader : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init() {
        if (!QDir(qtTranslations()).exists(QStringLiteral("qtbase_zh_CN.qm"))) {
            QSKIP("Qt is installed without its translations.");
        }
    }

    // The loader makes its locale the default locale. For English nothing is installed.
    void english_needs_no_translation() {
        TranslationLoader loader{QLocale(QLocale::English)};
        QCOMPARE(TranslationLoader::instance(), &loader);
        QCOMPARE(QLocale().language(), QLocale::English);
        QVERIFY(!loader.load(QStringLiteral("qtbase"), qtTranslations()));
        QCOMPARE(okText(), QStringLiteral("&OK"));
    }

    // A translation is installed by name, replaced by another load of the name, and removed by
    // name or with the loader.
    void the_translations_are_installed_by_name() {
        {
            TranslationLoader loader(QLocale(QStringLiteral("zh_CN")));
            QCOMPARE(QLocale().language(), QLocale::Chinese);
            QVERIFY(loader.load(QStringLiteral("qtbase"), qtTranslations()));
            QCOMPARE(okText(), QStringLiteral("确定(&O)"));
            loader.remove(QStringLiteral("qtbase"));
            QCOMPARE(okText(), QStringLiteral("&OK"));

            QVERIFY(loader.load(QStringLiteral("qtbase"), qtTranslations()));
            QVERIFY(loader.load(QStringLiteral("qtbase"), qtTranslations()));
            QVERIFY(!loader.load(QStringLiteral("missing"), qtTranslations()));
            QCOMPARE(okText(), QStringLiteral("确定(&O)"));
            loader.remove(QStringLiteral("qtbase"));
            QCOMPARE(okText(), QStringLiteral("&OK"));
            QVERIFY(loader.load(QStringLiteral("qtbase"), qtTranslations()));
        }
        QVERIFY(!TranslationLoader::instance());
        QCOMPARE(okText(), QStringLiteral("&OK"));
    }
};

QTEST_GUILESS_MAIN(test_TranslationLoader)

#include "test_TranslationLoader.moc"
