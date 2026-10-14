#include "TranslationLoader.h"

#include <algorithm>
#include <cassert>

#include <QtCore/QCoreApplication>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QTranslator>

namespace hello::kit {

    namespace {

        TranslationLoader *s_instance = nullptr;

        // The installed translator, which passes every lookup to the loaded translator one
        // thread at a time. QTranslator::translate() returns a null string if its lock is taken,
        // to avoid a deadlock during load(), and the lock is also taken while another thread
        // translates (qtranslator.cpp of Qt 6.11.1,
        // https://github.com/qt/qtbase/blob/v6.11.1/src/corelib/kernel/qtranslator.cpp). Texts
        // translated on worker threads, such as the warnings of the synth tools, would otherwise
        // remain in English intermittently. The loaded translator is complete before it is
        // installed, so that no lookup reaches it during load().
        class SerializedTranslator : public QTranslator {
        public:
            QTranslator loaded;

            QString translate(const char *context, const char *sourceText,
                              const char *disambiguation, int n) const override {
                const QMutexLocker locker(&m_mutex);
                return loaded.translate(context, sourceText, disambiguation, n);
            }

            bool isEmpty() const override {
                return loaded.isEmpty();
            }

        private:
            mutable QMutex m_mutex;
        };

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
        auto translator = std::make_unique<SerializedTranslator>();
        if (!translator->loaded.load(m_locale, name, QStringLiteral("_"), directory) ||
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
