#include "SoftwareRelease.h"

#include <algorithm>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <stdcorelib/str.h>

namespace hello::kit {

    namespace {

        bool isVersionCharacter(QChar c) {
            return c == QLatin1Char('.') || (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
        }

    }

    std::optional<SoftwareRelease> SoftwareRelease::fromJson(const QJsonObject &object,
                                                             const QString &pageTemplate) {
        // GitHub reports drafts only to the owners of the repository, and the latest release of
        // GitHub and Gitee excludes drafts and prereleases. Both fields are still checked for the
        // releases of other hosts and interfaces.
        if (object.value(QLatin1String("draft")).toBool() ||
            object.value(QLatin1String("prerelease")).toBool()) {
            return std::nullopt;
        }
        SoftwareRelease release;
        release.tag = object.value(QLatin1String("tag_name")).toString();
        const auto version = versionOfTag(release.tag);
        if (!version) {
            return std::nullopt;
        }
        release.version = *version;
        release.title = object.value(QLatin1String("name")).toString();
        release.notes = object.value(QLatin1String("body")).toString();
        const auto page = object.value(QLatin1String("html_url")).toString();
        release.page = page.isEmpty() ? pageOf(pageTemplate, release.tag) : QUrl(page);
        for (const auto item : object.value(QLatin1String("assets")).toArray()) {
            const auto asset = item.toObject();
            SoftwareReleaseAsset entry;
            entry.name = asset.value(QLatin1String("name")).toString();
            entry.url = QUrl(asset.value(QLatin1String("browser_download_url")).toString());
            if (entry.name.isEmpty() || !entry.url.isValid() ||
                entry.url.scheme() != QLatin1String("https")) {
                continue;
            }
            release.assets.push_back(std::move(entry));
        }
        return release;
    }

    QUrl SoftwareRelease::pageOf(const QString &pageTemplate, const QString &tag) {
        if (pageTemplate.isEmpty()) {
            return {};
        }
        const auto encodedTag = QUrl::toPercentEncoding(tag).toStdString();
        const auto page = stdc::str::varexp(pageTemplate.toStdString(),
                                            [&encodedTag](const std::string_view &name) {
                                                return name == "tag" ? encodedTag : std::string();
                                            });
        return QUrl(QString::fromStdString(page));
    }

    std::optional<QVersionNumber> SoftwareRelease::versionOfTag(const QString &tag) {
        if (!tag.startsWith(QLatin1Char('v'))) {
            return std::nullopt;
        }
        const auto segments = QStringView(tag).mid(1);
        // QVersionNumber::fromString() accepts a segment with a leading space or plus sign, such
        // as in "v 1" and "v+1", and reports only a trailing suffix in suffixIndex. Only digits
        // and dots are therefore accepted.
        if (segments.isEmpty() ||
            !std::all_of(segments.begin(), segments.end(), isVersionCharacter)) {
            return std::nullopt;
        }
        qsizetype suffixIndex = 0;
        auto version = QVersionNumber::fromString(segments, &suffixIndex);
        if (version.isNull() || suffixIndex != segments.size()) {
            return std::nullopt;
        }
        return version;
    }

    std::optional<SoftwareReleaseAsset> SoftwareRelease::asset(const QString &name) const {
        for (const auto &entry : assets) {
            if (entry.name == name) {
                return entry;
            }
        }
        return std::nullopt;
    }

}
