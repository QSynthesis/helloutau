#include "Sha256Sums.h"

#include <algorithm>

#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>

namespace hello::kit {

    namespace {

        constexpr qsizetype digestLength = 64;

        bool isHexDigit(char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        }

    }

    std::optional<QByteArray> Sha256Sums::digest(const QByteArray &sums, const QString &fileName) {
        const auto name = fileName.toUtf8();
        for (auto line : sums.split('\n')) {
            if (line.endsWith('\r')) {
                line.chop(1);
            }
            // The digest, a space, and a space for text mode or an asterisk for binary mode.
            // sha256sum escapes a name that contains a backslash or a line break, and marks the
            // line with a leading backslash. Such a name is not that of an asset.
            if (line.size() <= digestLength + 2) {
                continue;
            }
            const auto hex = line.first(digestLength);
            if (!std::all_of(hex.begin(), hex.end(), isHexDigit) || line[digestLength] != ' ' ||
                (line[digestLength + 1] != ' ' && line[digestLength + 1] != '*')) {
                continue;
            }
            if (line.sliced(digestLength + 2) == name) {
                return hex.toLower();
            }
        }
        return std::nullopt;
    }

    std::optional<QByteArray> Sha256Sums::fileDigest(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return std::nullopt;
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file)) {
            return std::nullopt;
        }
        return hash.result().toHex();
    }

}
