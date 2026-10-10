#include "Translations.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QPointer>
#include <QtCore/QTranslator>

namespace hello::daw {

    namespace {

        // The translators that install() and load() installed, which the next install() removes
        QList<QPointer<QTranslator>> s_installed;

    }

    QList<std::pair<QString, QString>> Translations::languages() {
        return {
            {QString(),               tr("System Default")      },
            {QStringLiteral("en"),    QStringLiteral("English") },
            {QStringLiteral("zh_CN"), QStringLiteral("简体中文")},
        };
    }

    QLocale Translations::localeOf(const QString &language) {
        return language.isEmpty() ? QLocale::system() : QLocale(language);
    }

    void Translations::install(const QString &language) {
        for (const auto &translator : std::as_const(s_installed)) {
            if (translator) {
                QCoreApplication::removeTranslator(translator);
                delete translator;
            }
        }
        s_installed.clear();
        QLocale::setDefault(localeOf(language));
        // Qt has no file for English, and the file of another language is installed only if Qt
        // ships it, so that the standard buttons and dialogs match the rest. The install rule of
        // helloutau/CMakeLists.txt puts the files of Qt under share/Qt/translations beside bin.
        const auto packaged = QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("../share/Qt/translations"));
        const auto system = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
        if (!load(QStringLiteral("qtbase"), QDir::cleanPath(packaged)) &&
            QDir::cleanPath(packaged) != QDir::cleanPath(system)) {
            load(QStringLiteral("qtbase"), system);
        }
        load(QStringLiteral("helloutau"), QStringLiteral(":/helloutau/translations"));
    }

    bool Translations::load(const QString &name, const QString &directory) {
        const QLocale locale;
        if (locale.language() == QLocale::English) {
            return false;
        }
        auto translator = new QTranslator(QCoreApplication::instance());
        if (!translator->load(locale, name, QStringLiteral("_"), directory) ||
            !QCoreApplication::installTranslator(translator)) {
            delete translator;
            return false;
        }
        s_installed.push_back(translator);
        return true;
    }

}
