#include "UtauFolderSearch.h"

#include <iterator>
#include <system_error>

#include <QtCore/QDir>
#include <QtCore/QThread>
#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN
#  include <QtCore/qt_windows.h>
#  include <msi.h>
#  include <winver.h>
#endif

namespace fs = std::filesystem;

namespace hello::daw {

    namespace {

        // The program whose folder is a UTAU folder
        constexpr char16_t programName[] = u"utau.exe";

        // The product name in the version resource of the original UTAU, 0.4.18 and 0.4.19
        constexpr char16_t originalProductName[] = u"歌声合成ツール UTAU";

        bool isInterrupted() {
            return QThread::currentThread()->isInterruptionRequested();
        }

        // Returns the path of utau.exe in folder, in any letter case, which a case-sensitive
        // file system of Wine may hold.
        std::optional<fs::path> programIn(const fs::path &folder) {
            std::error_code error;
            for (fs::directory_iterator it(folder, error), end; !error && it != end;
                 it.increment(error)) {
                const auto name = QString::fromStdU16String(it->path().filename().u16string());
                if (name.compare(QStringView(programName), Qt::CaseInsensitive) == 0 &&
                    it->is_regular_file(error)) {
                    return it->path();
                }
            }
            return std::nullopt;
        }

#ifdef Q_OS_WIN
        // The codes of utaustup.msi 0.4.18 and 0.4.19, see docs/UtauSearch.md
        constexpr wchar_t upgradeCode[] = L"{BB390196-0E40-444A-82E2-E7A8A02A99F5}";
        constexpr wchar_t programComponent[] = L"{14A967E4-7732-B0D9-1011-92F5FDA8A4EC}";

        // Returns the folders of utau.exe in the products of the official installer.
        QList<fs::path> installedFolders() {
            QList<fs::path> folders;
            wchar_t product[39];
            for (DWORD index = 0;
                 MsiEnumRelatedProductsW(upgradeCode, 0, index, product) == ERROR_SUCCESS;
                 ++index) {
                wchar_t path[MAX_PATH * 4];
                DWORD length = DWORD(std::size(path));
                if (MsiGetComponentPathW(product, programComponent, path, &length) ==
                    INSTALLSTATE_LOCAL) {
                    folders.push_back(fs::path(path).parent_path());
                }
            }
            return folders;
        }

        // Returns the roots of the fixed disks. A network disk, an optical drive or a removable
        // disk may not respond for long.
        QList<fs::path> fixedDrives() {
            QList<fs::path> drives;
            const DWORD mask = GetLogicalDrives();
            for (int letter = 0; letter < 26; ++letter) {
                if (!(mask & (DWORD(1) << letter))) {
                    continue;
                }
                const wchar_t root[] = {wchar_t(L'A' + letter), L':', L'\\', L'\0'};
                if (GetDriveTypeW(root) == DRIVE_FIXED) {
                    drives.push_back(fs::path(root));
                }
            }
            return drives;
        }

        // Returns the value names of key of the current user, or none if it is absent.
        QStringList valueNames(const wchar_t *key) {
            QStringList names;
            HKEY handle = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &handle) != ERROR_SUCCESS) {
                return names;
            }
            // The longest value name of the registry
            std::wstring name(16384, L'\0');
            for (DWORD index = 0;; ++index) {
                DWORD length = DWORD(name.size());
                const auto result = RegEnumValueW(handle, index, name.data(), &length, nullptr,
                                                  nullptr, nullptr, nullptr);
                if (result == ERROR_NO_MORE_ITEMS) {
                    break;
                }
                if (result == ERROR_SUCCESS) {
                    names.push_back(QString::fromWCharArray(name.data(), int(length)));
                }
            }
            RegCloseKey(handle);
            return names;
        }

        // Returns the string value at name of the version resource block, the first translation
        // of which is used, or an empty string.
        QString versionString(const QByteArray &block, const QString &name) {
            struct Translation {
                WORD language;
                WORD codePage;
            };
            Translation *translations = nullptr;
            UINT size = 0;
            if (!VerQueryValueW(block.constData(), L"\\VarFileInfo\\Translation",
                                reinterpret_cast<void **>(&translations), &size) ||
                size < sizeof(Translation)) {
                return {};
            }
            const auto path = QStringLiteral("\\StringFileInfo\\%1%2\\%3")
                                  .arg(translations->language, 4, 16, QLatin1Char('0'))
                                  .arg(translations->codePage, 4, 16, QLatin1Char('0'))
                                  .arg(name);
            wchar_t *value = nullptr;
            UINT length = 0;
            if (!VerQueryValueW(block.constData(), reinterpret_cast<LPCWSTR>(path.utf16()),
                                reinterpret_cast<void **>(&value), &length) ||
                length == 0) {
                return {};
            }
            // The value ends with a null character.
            return QString::fromWCharArray(value).trimmed();
        }

        // Returns the version of program as UtauFolderSearch::versionText() writes it, or an
        // empty string if the program has no version resource.
        QString versionOf(const fs::path &program) {
            DWORD handle = 0;
            const DWORD size = GetFileVersionInfoSizeW(program.c_str(), &handle);
            if (size == 0) {
                return {};
            }
            QByteArray block(qsizetype(size), '\0');
            if (!GetFileVersionInfoW(program.c_str(), 0, size, block.data())) {
                return {};
            }
            const auto version = versionString(block, QStringLiteral("FileVersion"));
            return version.isEmpty()
                       ? QString()
                       : UtauFolderSearch::versionText(
                             version, versionString(block, QStringLiteral("ProductName")));
        }
#else
        // The version resource is not read without the API of Windows.
        QString versionOf(const fs::path &) {
            return {};
        }
#endif

    }

    UtauFolderSearch::UtauFolderSearch(QObject *parent) : QObject(parent) {
    }

    UtauFolderSearch::~UtauFolderSearch() = default;

    void UtauFolderSearch::run() {
        m_folders.clear();
        const auto offerAll = [this](const QList<fs::path> &folders, Source source) {
            for (const auto &folder : folders) {
                if (!offer(folder, source)) {
                    return false;
                }
            }
            return true;
        };
#ifdef Q_OS_WIN
        // The local application data folder of the user, into which a program is installed for
        // the user alone
        const auto localAppData = fs::path(qEnvironmentVariable("LOCALAPPDATA").toStdU16String());
        if (offerAll(installedFolders(), Installer) &&
            offerAll(commonLocations(fixedDrives(), localAppData), CommonLocation)) {
            offerAll(
                foldersOfPrograms(
                    valueNames(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags"
                               L"\\Compatibility Assistant\\Store") +
                    valueNames(L"SOFTWARE\\Classes\\Local Settings\\Software\\Microsoft\\Windows"
                               L"\\Shell\\MuiCache")),
                RunHistory);
        }
#else
        auto prefix = qEnvironmentVariable("WINEPREFIX");
        if (prefix.isEmpty()) {
            prefix = QDir::homePath() + QStringLiteral("/.wine");
        }
        offerAll(wineLocations(fs::path(prefix.toStdU16String())), Wine);
#endif
        Q_EMIT finished();
    }

    QString UtauFolderSearch::sourceName(Source source) {
        switch (source) {
            case Installer:
                return tr("Installer");
            case CommonLocation:
                return tr("Common location");
            case RunHistory:
                return tr("Run history");
            case Wine:
                return tr("Wine");
        }
        return {};
    }

    QList<fs::path> UtauFolderSearch::commonLocations(const QList<fs::path> &drives,
                                                      const fs::path &localAppData) {
        QList<fs::path> folders;
        for (const auto &drive : drives) {
            folders.push_back(drive / u"Program Files (x86)" / u"UTAU");
            folders.push_back(drive / u"Program Files" / u"UTAU");
            folders.push_back(drive / u"UTAU");
        }
        if (!localAppData.empty()) {
            folders.push_back(localAppData / u"Programs" / u"UTAU");
        }
        return folders;
    }

    QList<fs::path> UtauFolderSearch::wineLocations(const fs::path &prefix) {
        const auto drive = prefix / u"drive_c";
        return {drive / u"Program Files (x86)" / u"UTAU", drive / u"Program Files" / u"UTAU",
                drive / u"UTAU"};
    }

    QList<fs::path> UtauFolderSearch::foldersOfPrograms(const QStringList &names) {
        QList<fs::path> folders;
        QStringList seen;
        for (const auto &name : names) {
            // The path ends at the last .exe, which a suffix of the MuiCache follows
            const auto end = name.lastIndexOf(QLatin1String(".exe"), -1, Qt::CaseInsensitive);
            if (end < 0) {
                continue;
            }
            // The names are paths of Windows on every system, whereas fromNativeSeparators() of
            // QDir converts only the separators of this system.
            auto program = name.left(end + 4);
            program.replace(u'\\', u'/');
            const auto slash = program.lastIndexOf(u'/');
            if (slash < 0 || program.mid(slash + 1).compare(QStringView(programName),
                                                            Qt::CaseInsensitive) != 0) {
                continue;
            }
            const auto folder = program.left(slash);
            if (!seen.contains(folder, Qt::CaseInsensitive)) {
                seen.push_back(folder);
                folders.push_back(fs::path(folder.toStdU16String()));
            }
        }
        return folders;
    }

    QString UtauFolderSearch::versionText(const QString &fileVersion, const QString &productName) {
        QStringList parts;
        for (const auto &part : fileVersion.split(u'.')) {
            bool number = false;
            const int value = part.trimmed().toInt(&number);
            if (!number) {
                parts.clear();
                break;
            }
            parts.push_back(QString::number(value));
        }
        const auto version = parts.isEmpty() ? fileVersion : parts.join(u'.');
        return productName.isEmpty() || productName == QStringView(originalProductName)
                   ? version
                   : QStringLiteral("%1 (%2)").arg(version, productName);
    }

    std::optional<UtauFolderSearch::Candidate> UtauFolderSearch::candidateOf(const fs::path &folder,
                                                                             Source source) {
        const auto program = programIn(folder);
        if (!program) {
            return std::nullopt;
        }
        return Candidate{folder, source, versionOf(*program)};
    }

    bool UtauFolderSearch::offer(const fs::path &folder, Source source) {
        if (isInterrupted()) {
            return false;
        }
        std::error_code error;
        for (const auto &found : std::as_const(m_folders)) {
            if (fs::equivalent(found, folder, error)) {
                return true;
            }
        }
        if (auto candidate = candidateOf(folder, source)) {
            m_folders.push_back(folder);
            Q_EMIT found(*candidate);
        }
        return true;
    }

}
