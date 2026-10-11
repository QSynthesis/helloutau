#include "TranslationLoader.h"

#include <algorithm>
#include <cassert>

#include <QtCore/QCoreApplication>
#include <QtCore/QTranslator>

namespace hello::kit {

    namespace {

        TranslationLoader *s_instance = nullptr;

    }

    TranslationLoader::TranslationLoader(const QLocale &locale) : m_locale(locale) {
        assert(!s_instance);
        s_instance = this;
        QLocale::setDefault(locale);
    }

    TranslationLoader::~TranslationLoader() {
        for (const auto &[name, translator] : m_translators) {
            QCoreApplication::removeTranslator(translator.get());
        }
        s_instance = nullptr;
    }

    TranslationLoader *TranslationLoader::instance() {
        return s_instance;
    }

    bool TranslationLoader::load(const QString &name, const QString &directory) {
        remove(name);
        // The sources are in English, and Qt has no file for English either.
        if (m_locale.language() == QLocale::English) {
            return false;
        }
        auto translator = std::make_unique<QTranslator>();
        if (!translator->load(m_locale, name, QStringLiteral("_"), directory) ||
            !QCoreApplication::installTranslator(translator.get())) {
            return false;
        }
        // A deep copy, because the name may be a literal in the library of a plugin, whose data
        // is unloaded with the library
        m_translators.emplace_back(QString(name.constData(), name.size()), std::move(translator));
        return true;
    }

    void TranslationLoader::remove(const QString &name) {
        const auto it = std::find_if(m_translators.begin(), m_translators.end(),
                                     [&name](const auto &entry) { return entry.first == name; });
        if (it == m_translators.end()) {
            return;
        }
        QCoreApplication::removeTranslator(it->second.get());
        m_translators.erase(it);
    }

}
