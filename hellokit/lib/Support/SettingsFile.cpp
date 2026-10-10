#include "SettingsFile.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>
#include <QtCore/QtDebug>

namespace hello::kit {

    namespace json = stdc::json;

    json::Object SettingsFile::read(const QString &fileName) {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        const auto text = file.readAll();
        json::ParseError error;
        auto document = json::Value::fromJson(std::string_view(text.data(), size_t(text.size())),
                                              false, &error);
        if (const auto object = document.asObject()) {
            return std::move(*object);
        }
        qWarning().noquote() << "The settings in" << fileName
                             << "cannot be read:" << QString::fromStdString(error.message());
        return {};
    }

    SettingsFile::SettingsFile(QString fileName, std::function<json::Value()> content)
        : m_fileName(std::move(fileName)), m_content(std::move(content)) {
        m_timer.setSingleShot(true);
        m_timer.setInterval(0);
        QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this] { sync(); });
    }

    SettingsFile::~SettingsFile() {
        sync();
    }

    void SettingsFile::changed() {
        if (m_pending) {
            return;
        }
        m_pending = true;
        // Without an application instance, no event loop exists to wait for.
        if (QCoreApplication::instance()) {
            m_timer.start();
        } else {
            sync();
        }
    }

    void SettingsFile::sync() {
        if (!m_pending) {
            return;
        }
        m_pending = false;
        m_timer.stop();
        write(m_content());
    }

    void SettingsFile::write(const json::Value &value) const {
        QDir().mkpath(QFileInfo(m_fileName).absolutePath());
        const auto text = value.toJson(4);
        QSaveFile file(m_fileName);
        if (!file.open(QIODevice::WriteOnly) || file.write(text.data(), qint64(text.size())) < 0 ||
            !file.commit()) {
            qWarning().noquote() << "The settings cannot be written to" << m_fileName << ":"
                                 << file.errorString();
        }
    }

}
