#include "EngineTrust_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtWidgets/QMessageBox>

#include "AppSettings.h"

namespace hello::daw::EngineTrust {

    namespace {
        class Messages {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ProjectPropertiesDialog)
        };
        constexpr char Key[] = "engineTrust/approved";

        QString textOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        QString kindText(Kind kind) {
            return kind == Kind::Wavtool ? Messages::tr("wavtool") : Messages::tr("resampler");
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

    std::filesystem::path resolved(const QString &value, const std::filesystem::path &utau) {
        return pathOf(value, utau);
    }

    bool exists(const QString &value, const std::filesystem::path &utau) {
        std::error_code error;
        const auto path = pathOf(value, utau);
        return !path.empty() && std::filesystem::is_regular_file(path, error);
    }

    bool samePath(const QString &first, const QString &second, const std::filesystem::path &utau) {
        std::error_code firstError;
        std::error_code secondError;
        const auto a = std::filesystem::weakly_canonical(pathOf(first, utau), firstError);
        const auto b = std::filesystem::weakly_canonical(pathOf(second, utau), secondError);
        return !firstError && !secondError && !a.empty() && a == b;
    }

    bool isTrusted(const AppSettings &settings, const QString &value,
                   const std::filesystem::path &utau, Kind kind) {
        Q_UNUSED(kind);
        const auto path = pathOf(value, utau);
        if (path.empty() || !exists(value, utau)) {
            return false;
        }
        std::error_code error;
        const auto absolute = std::filesystem::weakly_canonical(path, error);
        const auto filePath = textOf(absolute);
        for (const auto &item : entries(settings)) {
            const auto object = item.toObject();
            if (object.value(QStringLiteral("filePath")).toString() == filePath) {
                return true;
            }
        }
        return false;
    }

    void trust(AppSettings &settings, const QString &value, const std::filesystem::path &utau,
               Kind kind) {
        const auto path = pathOf(value, utau);
        std::error_code error;
        const auto absolute = std::filesystem::weakly_canonical(path, error);
        if (error || absolute.empty() || !exists(value, utau)) {
            return;
        }
        auto array = entries(settings);
        Q_UNUSED(kind);
        const auto filePath = textOf(absolute);
        for (const auto &item : array) {
            if (item.toObject().value(QStringLiteral("filePath")).toString() == filePath) {
                return;
            }
        }
        array.push_back(QJsonObject{
            {QStringLiteral("filePath"), filePath}
        });
        settings.setValue(QLatin1String(Key), array);
    }

    bool ask(QWidget *parent, AppSettings &settings, const QString &value,
             const std::filesystem::path &utau, Kind kind) {
        if (!exists(value, utau) || isTrusted(settings, value, utau, kind)) {
            return isTrusted(settings, value, utau, kind);
        }
        const auto path = pathOf(value, utau);
        const auto answer = QMessageBox::question(
            parent, Messages::tr("Trust Project Engine"),
            Messages::tr("The project requests this %1:\n\n%2\n\nTrust and run it?")
                .arg(kindText(kind), textOf(path)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return false;
        }
        trust(settings, value, utau, kind);
        return true;
    }
}
