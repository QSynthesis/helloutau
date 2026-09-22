#include "VoiceBankConfig.h"

#include <fstream>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonParseError>

namespace hello::kit {

    namespace {

        constexpr char KeyFormat[] = "$format";
        constexpr char KeyCharset[] = "charset";

        constexpr char FormatName[] = "hello-voicebank";

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
        }

    }

    std::optional<VoiceBankConfig> VoiceBankConfig::open(const std::filesystem::path &path,
                                                         DiagnosticList &diagnostics) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            fail(diagnostics, tr("This file could not be opened."));
            return std::nullopt;
        }
        const std::string bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        return fromJson(QByteArrayView(bytes.data(), qsizetype(bytes.size())), diagnostics);
    }

    bool VoiceBankConfig::save(const std::filesystem::path &path,
                               DiagnosticList &diagnostics) const {
        const auto bytes = toJson();

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            fail(diagnostics, tr("This file could not be written."));
            return false;
        }
        out.write(bytes.constData(), bytes.size());
        if (!out) {
            fail(diagnostics, tr("This file could not be written."));
            return false;
        }
        return true;
    }

    std::optional<VoiceBankConfig> VoiceBankConfig::fromJson(QByteArrayView json,
                                                             DiagnosticList &diagnostics) {
        QJsonParseError error{};
        const auto document = QJsonDocument::fromJson(json.toByteArray(), &error);
        if (error.error != QJsonParseError::NoError) {
            fail(diagnostics, tr("This file is not valid JSON: %1").arg(error.errorString()));
            return std::nullopt;
        }
        if (!document.isObject()) {
            fail(diagnostics, tr("This file is not a HelloUTAU voice bank configuration."));
            return std::nullopt;
        }

        auto root = document.object();
        if (root.value(QLatin1String(KeyFormat)).toString() != QLatin1String(FormatName)) {
            fail(diagnostics, tr("This file is not a HelloUTAU voice bank configuration."));
            return std::nullopt;
        }

        VoiceBankConfig config;
        config.charset = root.value(QLatin1String(KeyCharset)).toString();

        root.remove(QLatin1String(KeyFormat));
        root.remove(QLatin1String(KeyCharset));
        config.unknownFields = root;
        return config;
    }

    QByteArray VoiceBankConfig::toJson() const {
        QJsonObject root = unknownFields;
        root.insert(QLatin1String(KeyFormat), QLatin1String(FormatName));
        root.insert(QLatin1String(KeyCharset), charset);
        return QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

}
