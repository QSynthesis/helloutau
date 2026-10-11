#include "SoftwareRelease.h"

#include <algorithm>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

namespace hello::kit {

    namespace {

        bool isVersionCharacter(QChar c) {
            return c == QLatin1Char('.') || (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
        }

    }

    std::optional<SoftwareRelease> SoftwareRelease::fromJson(const QJsonObject &object, Host host,
                                                             const QString &repository) {
        // GitHub reports drafts only to the owners of the repository, and both hosts omit
        // drafts and prereleases from the latest release. A release passed in from elsewhere is
        // checked all the same.
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
        if (host == GitHub) {
            release.page = QUrl(object.value(QLatin1String("html_url")).toString());
        } else {
            // The release of Gitee has no address of its page.
            release.page = QUrl(QStringLiteral("https://gitee.com/") + repository +
                                QStringLiteral("/releases/tag/") +
                                QString::fromLatin1(QUrl::toPercentEncoding(release.tag)));
        }
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
