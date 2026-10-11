#include "UpdateSettings.h"

#include "AppSettings_p.h"

namespace hello::daw {

    namespace json = stdc::json;

    namespace {

        constexpr char KeyMode[] = "updates/mode";
        constexpr char ManualValue[] = "manual";
        constexpr char AutomaticValue[] = "automatic";
        constexpr char KeySource[] = "updates/source";
        constexpr char KeyLastCheck[] = "updates/lastCheck";
        constexpr char KeySkippedVersion[] = "updates/skippedVersion";
        constexpr char KeyPending[] = "updates/pending";
        constexpr char KeyPendingVersion[] = "updates/pending/version";
        constexpr char KeyPendingPage[] = "updates/pending/page";

    }

    UpdateSettings::UpdateSettings(AppSettingsStore &store) : m_store(store) {
    }

    UpdateSettings::~UpdateSettings() = default;

    UpdateSettings::Mode UpdateSettings::mode() const {
        const auto value = m_store.value(KeyMode).toString();
        if (value == ManualValue) {
            return Manual;
        }
        return value == AutomaticValue ? Automatic : Notified;
    }

    void UpdateSettings::setMode(Mode mode) {
        // The default is not written.
        json::Value value;
        if (mode == Manual) {
            value = std::string(ManualValue);
        } else if (mode == Automatic) {
            value = std::string(AutomaticValue);
        }
        m_store.setValue(KeyMode, std::move(value));
    }

    QString UpdateSettings::source() const {
        return m_store.text(KeySource);
    }

    void UpdateSettings::setSource(const QString &source) {
        m_store.setValue(KeySource,
                         source.isEmpty() ? json::Value() : json::Value(source.toStdString()));
    }

    QDateTime UpdateSettings::lastCheck() const {
        return QDateTime::fromString(m_store.text(KeyLastCheck), Qt::ISODate);
    }

    void UpdateSettings::setLastCheck(const QDateTime &time) {
        m_store.setValue(KeyLastCheck,
                         time.isValid()
                             ? json::Value(time.toUTC().toString(Qt::ISODate).toStdString())
                             : json::Value());
    }

    QString UpdateSettings::skippedVersion() const {
        return m_store.text(KeySkippedVersion);
    }

    void UpdateSettings::setSkippedVersion(const QString &version) {
        m_store.setValue(KeySkippedVersion,
                         version.isEmpty() ? json::Value() : json::Value(version.toStdString()));
    }

    QString UpdateSettings::pendingVersion() const {
        return m_store.text(KeyPendingVersion);
    }

    QUrl UpdateSettings::pendingPage() const {
        return QUrl(m_store.text(KeyPendingPage));
    }

    void UpdateSettings::setPendingInstallation(const QString &version, const QUrl &page) {
        m_store.setValue(KeyPendingVersion, version.toStdString());
        m_store.setValue(KeyPendingPage, page.toString().toStdString());
    }

    void UpdateSettings::clearPendingInstallation() {
        m_store.setValue(KeyPending, json::Value());
    }

}
