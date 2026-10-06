#include "EngineTrust.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtWidgets/QMessageBox>

#include "AppSettings.h"

namespace hello::daw {

    namespace {

        class Messages {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ProjectPropertiesDialog)
        };
        constexpr char Key[] = "engineTrust/approved";

        QString textOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        QString sha256Of(const std::filesystem::path &path) {
            QFile file(textOf(path));
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file)) {
                return {};
            }
            return QString::fromLatin1(hash.result().toHex());
        }

        QJsonArray entries(const AppSettings &settings) {
            return settings.value(QLatin1String(Key)).toArray();
        }

        std::filesystem::path pathOf(const QString &value, const std::filesystem::path &utau) {
            const std::filesystem::path path(value.toStdU16String());
            if (path.empty()) {
                return {};
            }
            return path.is_absolute() || utau.empty() ? path : utau / path;
        }

    }

    std::filesystem::path EngineTrust::resolved(const QString &value,
                                                const std::filesystem::path &utau) {
        return pathOf(value, utau);
    }

    bool EngineTrust::exists(const QString &value, const std::filesystem::path &utau) {
        std::error_code error;
        const auto path = pathOf(value, utau);
        return !path.empty() && std::filesystem::is_regular_file(path, error);
    }

    bool EngineTrust::samePath(const QString &first, const QString &second,
                               const std::filesystem::path &utau) {
        std::error_code firstError;
        std::error_code secondError;
        const auto a = std::filesystem::weakly_canonical(pathOf(first, utau), firstError);
        const auto b = std::filesystem::weakly_canonical(pathOf(second, utau), secondError);
        return !firstError && !secondError && !a.empty() && a == b;
    }

    bool EngineTrust::isTrusted(const AppSettings &settings, const QString &value,
                                const std::filesystem::path &utau) {
        const auto path = pathOf(value, utau);
        if (path.empty() || !exists(value, utau)) {
            return false;
        }
        std::error_code error;
        const auto absolute = std::filesystem::weakly_canonical(path, error);
        if (error || absolute.empty()) {
            return false;
        }
        const auto filePath = textOf(absolute);
        const auto sha256 = sha256Of(absolute);
        if (sha256.isEmpty()) {
            return false;
        }
        for (const auto &item : entries(settings)) {
            const auto object = item.toObject();
            if (object.value(QStringLiteral("filePath")).toString() == filePath &&
                object.value(QStringLiteral("sha256")).toString() == sha256) {
                return true;
            }
        }
        return false;
    }

    void EngineTrust::trust(AppSettings &settings, const QString &value,
                            const std::filesystem::path &utau) {
        const auto path = pathOf(value, utau);
        std::error_code error;
        const auto absolute = std::filesystem::weakly_canonical(path, error);
        if (error || absolute.empty() || !exists(value, utau)) {
            return;
        }
        const auto filePath = textOf(absolute);
        const auto sha256 = sha256Of(absolute);
        if (sha256.isEmpty()) {
            return;
        }
        auto array = entries(settings);
        for (qsizetype i = array.size(); i-- > 0;) {
            if (array.at(i).toObject().value(QStringLiteral("filePath")).toString() == filePath) {
                array.removeAt(i);
            }
        }
        array.push_back(QJsonObject{
            {QStringLiteral("filePath"), filePath},
            {QStringLiteral("sha256"), sha256}
        });
        settings.setValue(QLatin1String(Key), array);
    }

    bool EngineTrust::ask(QWidget *parent, AppSettings &settings, const QString &value,
                          const std::filesystem::path &utau) {
        if (!exists(value, utau) || isTrusted(settings, value, utau)) {
            return isTrusted(settings, value, utau);
        }
        const auto path = pathOf(value, utau);
        const auto answer = QMessageBox::question(
            parent, Messages::tr("Trust Project Engine"),
            Messages::tr("The project requests this rendering tool:\n\n%1\n\nTrust and run it?")
                .arg(textOf(path)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return false;
        }
        trust(settings, value, utau);
        return true;
    }

}
