#ifndef HELLOKIT_SUPPORT_ZIPEXTRACTOR_H
#define HELLOKIT_SUPPORT_ZIPEXTRACTOR_H

#include <atomic>
#include <filesystem>
#include <functional>
#include <future>
#include <optional>

#include <QtCore/QByteArrayView>
#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// The extraction of a zip archive into a directory. The names of the entries are untrusted
    /// input, and every entry is checked before any file is written. The archive is rejected as a
    /// whole, not filtered, if one entry fails a check:
    ///
    /// - The name is decoded as UTF-8 if the entry is marked as UTF-8 or the name is ASCII.
    ///   Otherwise the name decoder decodes it, and the name is rejected if no decoder is set or
    ///   the decoder returns \c std::nullopt.
    /// - Both the slash and the backslash separate the components of a name. A name that begins
    ///   with a separator, or has a component that is empty, \c . or \c .., or contains a colon or
    ///   a control character, is rejected. A colon covers drive letters and the alternate data
    ///   streams of NTFS. On Windows a component that is a reserved device name, such as
    ///   \c CON or \c nul.txt, or that ends with a dot or a space, is also rejected.
    /// - A symbolic link or an encrypted entry is rejected.
    /// - The target of each entry must lie within the directory after
    ///   \c std::filesystem::weakly_canonical, which resolves the existing symbolic links.
    /// - The number of entries and the sum of the declared sizes must not exceed the limits. The
    ///   bytes actually written are counted as well, because a declared size can be forged.
    ///
    /// On systems other than Windows the permission bits of an entry written on Unix are applied
    /// to its file, without the set-user-ID, set-group-ID and sticky bits.
    class HELLOKIT_SUPPORT_EXPORT ZipExtractor {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::ZipExtractor)
    public:
        enum Error {
            NoError,
            OpenError,
            TooManyEntries,
            TooLarge,
            UndecodableName,
            UnsafePath,
            SymbolicLink,
            EncryptedEntry,
            ReadError,
            WriteError,
            Canceled,
        };

        /// Decodes the raw name of an entry that is neither marked as UTF-8 nor ASCII, and
        /// returns \c std::nullopt if the name is invalid in the encoding.
        using NameDecoder = std::function<std::optional<QString>(QByteArrayView name)>;

        /// Receives the number of bytes written and the sum of the declared sizes after each
        /// written block, and returns whether the extraction continues. With several threads
        /// the handler is called on each of them, one call at a time.
        using ProgressHandler = std::function<bool(qint64 written, qint64 total)>;

        static constexpr qint64 defaultMaximumEntryCount = 65536;
        static constexpr qint64 defaultMaximumSize = qint64(4) << 30;

        ZipExtractor();
        ~ZipExtractor();

        /// Sets the decoder of the names that are neither marked as UTF-8 nor ASCII. Without a
        /// decoder such a name is rejected.
        void setNameDecoder(NameDecoder decoder);

        qint64 maximumEntryCount() const;
        void setMaximumEntryCount(qint64 count);

        /// The largest number of bytes written in total
        qint64 maximumSize() const;
        void setMaximumSize(qint64 size);

        /// Whether the single directory that holds every entry is removed from the paths, as in
        /// an archive that holds one folder of the same name
        bool isTopDirectoryStripped() const;
        void setTopDirectoryStripped(bool stripped);

        /// The number of threads that write the files, 1 by default. Each thread opens the
        /// archive once, and a count below 1 is treated as 1. The checks run on the calling
        /// thread before any file is written.
        int threadCount() const;
        void setThreadCount(int count);

        void setProgressHandler(ProgressHandler handler);

        /// Extracts \a archive into \a directory, which is created if absent. Existing files of
        /// the same names are replaced.
        ///
        /// \return whether every entry was extracted. If an entry fails a check, nothing is
        ///         written. If reading or writing fails later, the files already written remain,
        ///         and the caller removes them. error() and errorString() return the reason.
        bool extract(const std::filesystem::path &archive, const std::filesystem::path &directory);

        /// Extracts \a archive into \a directory as extract() does, on a new thread, and returns
        /// the result of extract() as a future. The extractor must outlive the extraction, and
        /// is neither modified nor used for another extraction until the future is ready. As
        /// with \c std::async, destroying the future waits for the extraction to finish.
        std::future<bool> extractAsync(const std::filesystem::path &archive,
                                       const std::filesystem::path &directory);

        /// Requests that the running extraction stops. The extraction fails with \c Canceled at
        /// the next entry that it checks or the next block that it writes. This function may be
        /// called from any thread. A request before extract() or extractAsync() is discarded.
        void cancel();

        /// Returns the reason why the last extraction failed, or \c NoError if it succeeded.
        Error error() const;

        /// Returns the description of error() for the user, or an empty string if the last
        /// extraction succeeded.
        QString errorString() const;

    private:
        bool run(const std::filesystem::path &archive, const std::filesystem::path &directory);
        bool fail(Error error, const QString &message);

        NameDecoder m_nameDecoder;
        ProgressHandler m_progressHandler;
        qint64 m_maximumEntryCount = defaultMaximumEntryCount;
        qint64 m_maximumSize = defaultMaximumSize;
        int m_threadCount = 1;
        bool m_topDirectoryStripped = false;
        std::atomic<bool> m_canceled = false;
        Error m_error = NoError;
        QString m_errorString;
    };

}

#endif // HELLOKIT_SUPPORT_ZIPEXTRACTOR_H
