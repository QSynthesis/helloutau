#ifndef HELLOKIT_UPDATE_SHA256SUMS_H
#define HELLOKIT_UPDATE_SHA256SUMS_H

#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <hellokit/Update/HelloKitUpdateGlobal.h>

namespace hello::kit {

    /// The \c SHA256SUMS file of a release, by which a downloaded asset is verified. Each line
    /// holds the SHA-256 digest of one file in hexadecimal and the name of the file, as
    /// \c sha256sum writes them.
    class HELLOKIT_UPDATE_EXPORT Sha256Sums {
    public:
        /// Returns the digest of \a fileName in \a sums, the content of a \c SHA256SUMS file, in
        /// lowercase hexadecimal, or \c std::nullopt if \a sums lists no such file.
        static std::optional<QByteArray> digest(const QByteArray &sums, const QString &fileName);

        /// Returns the SHA-256 digest of the file at \a path in lowercase hexadecimal, or
        /// \c std::nullopt if the file cannot be read.
        static std::optional<QByteArray> fileDigest(const QString &path);
    };

}

#endif // HELLOKIT_UPDATE_SHA256SUMS_H
