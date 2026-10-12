#include "VoiceBankConfig.h"

#include <algorithm>
#include <fstream>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonParseError>

#include <hellokit/Support/TextCodec.h>

namespace hello::kit {

    namespace {

        constexpr char KeyFormat[] = "$format";
        constexpr char KeyCharset[] = "charset";
        constexpr char KeyFileNameEncoding[] = "fileNameEncoding";
        constexpr char KeyOriginal[] = "original";
        constexpr char KeyMisdecodedAs[] = "misdecodedAs";
        constexpr char KeyFiles[] = "files";
        constexpr char KeyMisdecoded[] = "misdecoded";
        constexpr char KeyCorrect[] = "correct";

        constexpr char FormatName[] = "hello-voicebank";

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
        }

        void complain(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Warning, message});
        }

        bool isAscii(QStringView text) {
            return std::all_of(text.begin(), text.end(),
                               [](QChar c) { return c.unicode() < 0x80; });
        }

        bool isNameCharacter(QChar c) {
            return c.unicode() >= 0x20 && c.unicode() != 0x7f && c != QLatin1Char('/') &&
                   c != QLatin1Char('\\');
        }

        /// The strings of \a value , or \c std::nullopt if it is not an array of strings.
        std::optional<QStringList> stringsOf(const QJsonValue &value) {
            if (!value.isArray()) {
                return std::nullopt;
            }
            QStringList strings;
            for (const auto &item : value.toArray()) {
                if (!item.isString()) {
                    return std::nullopt;
                }
                strings.push_back(item.toString());
            }
            return strings;
        }

        /// Returns whether every key of \a object is among \a keys .
        bool hasOnly(const QJsonObject &object, std::initializer_list<const char *> keys) {
            const auto objectKeys = object.keys();
            return std::all_of(objectKeys.begin(), objectKeys.end(), [&keys](const QString &key) {
                return std::any_of(keys.begin(), keys.end(), [&key](const char *known) {
                    return key == QLatin1String(known);
                });
            });
        }

    }

    std::optional<VoiceBankConfig::FileNameEncoding>
        VoiceBankConfig::FileNameEncoding::fromJson(const QJsonValue &json) {
        if (!json.isObject()) {
            return std::nullopt;
        }
        const auto object = json.toObject();
        if (!hasOnly(object, {KeyOriginal, KeyMisdecodedAs, KeyFiles})) {
            return std::nullopt;
        }
        const auto original = object.value(QLatin1String(KeyOriginal));
        const auto misdecodedAs = object.value(QLatin1String(KeyMisdecodedAs));
        FileNameEncoding record;
        if (original.isUndefined() && misdecodedAs.isUndefined()) {
            return object.contains(QLatin1String(KeyFiles)) ? std::nullopt : std::optional(record);
        }
        if (!original.isString() || !misdecodedAs.isString() || original.toString().isEmpty() ||
            misdecodedAs.toString().isEmpty()) {
            return std::nullopt;
        }
        record.original = original.toString();
        record.misdecodedAs = misdecodedAs.toString();
        if (!object.contains(QLatin1String(KeyFiles))) {
            return record;
        }
        const auto files = object.value(QLatin1String(KeyFiles)).toObject();
        if (!object.value(QLatin1String(KeyFiles)).isObject() ||
            !hasOnly(files, {KeyMisdecoded, KeyCorrect})) {
            return std::nullopt;
        }
        const auto misdecoded = stringsOf(files.value(QLatin1String(KeyMisdecoded)));
        const auto correct = stringsOf(files.value(QLatin1String(KeyCorrect)));
        if (!misdecoded || !correct) {
            return std::nullopt;
        }
        record.files = Files{*misdecoded, *correct};
        return record;
    }

    QJsonObject VoiceBankConfig::FileNameEncoding::toJson() const {
        QJsonObject object;
        if (isAsIs()) {
            return object;
        }
        object.insert(QLatin1String(KeyOriginal), original);
        object.insert(QLatin1String(KeyMisdecodedAs), misdecodedAs);
        if (files) {
            object.insert(
                QLatin1String(KeyFiles),
                QJsonObject{
                    {QLatin1String(KeyMisdecoded), QJsonArray::fromStringList(files->misdecoded)},
                    {QLatin1String(KeyCorrect),    QJsonArray::fromStringList(files->correct)   },
            });
        }
        return object;
    }

    std::optional<QString>
        VoiceBankConfig::FileNameEncoding::restore(const QString &physical) const {
        const auto name = physical.normalized(QString::NormalizationForm_C);
        if (isAscii(name)) {
            return std::nullopt;
        }
        const TextCodec from(misdecodedAs);
        const TextCodec to(original);
        if (!from.isValid() || !to.isValid() || !from.canEncode(name)) {
            return std::nullopt;
        }
        auto restored = to.decode(from.encode(name));
        if (!restored) {
            return std::nullopt;
        }
        *restored = restored->normalized(QString::NormalizationForm_C);
        if (*restored == name ||
            !std::all_of(restored->begin(), restored->end(), isNameCharacter)) {
            return std::nullopt;
        }
        return restored;
    }

    QString VoiceBankConfig::FileNameEncoding::logicalNameOf(const QString &physical) const {
        if (!files) {
            return restore(physical).value_or(physical);
        }
        const auto name = physical.normalized(QString::NormalizationForm_C);
        const auto &misdecoded = files->misdecoded;
        const bool listed =
            std::any_of(misdecoded.begin(), misdecoded.end(), [&name](const QString &item) {
                return item.normalized(QString::NormalizationForm_C) == name;
            });
        return listed ? restore(physical).value_or(physical) : physical;
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
            fail(diagnostics, tr("This file is not a HelloUtau voice bank configuration."));
            return std::nullopt;
        }

        auto root = document.object();
        if (root.value(QLatin1String(KeyFormat)).toString() != QLatin1String(FormatName)) {
            fail(diagnostics, tr("This file is not a HelloUtau voice bank configuration."));
            return std::nullopt;
        }

        VoiceBankConfig config;
        config.charset = root.value(QLatin1String(KeyCharset)).toString();

        root.remove(QLatin1String(KeyFormat));
        root.remove(QLatin1String(KeyCharset));

        // A malformed record stays among the unknown fields, so that a save writes it back
        // rather than removing it.
        if (root.contains(QLatin1String(KeyFileNameEncoding))) {
            config.fileNameEncoding =
                FileNameEncoding::fromJson(root.value(QLatin1String(KeyFileNameEncoding)));
            if (config.fileNameEncoding) {
                root.remove(QLatin1String(KeyFileNameEncoding));
            } else {
                complain(diagnostics, tr("The record of the file name encoding is malformed, so "
                                         "the file names are used as they are."));
            }
        }
        config.unknownFields = root;
        return config;
    }

    QByteArray VoiceBankConfig::toJson() const {
        QJsonObject root = unknownFields;
        root.insert(QLatin1String(KeyFormat), QLatin1String(FormatName));
        root.insert(QLatin1String(KeyCharset), charset);
        if (fileNameEncoding) {
            root.insert(QLatin1String(KeyFileNameEncoding), fileNameEncoding->toJson());
        }
        return QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

}
