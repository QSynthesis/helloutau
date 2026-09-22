#include "TextCodec.h"

#ifdef _WIN32
#  include <QtCore/qt_windows.h>
#endif

#include <QtCore/QStringConverter>
#include <QtCore/QStringDecoder>
#include <QtCore/QStringEncoder>

#include <winacp/winacp.h>

namespace hello::kit {

    namespace {

        // The encodings this project promises, by the Windows code page that holds each one.
        //
        // The canonical name is what gets recorded in a control note, so it is written out here
        // rather than taken from whichever library happened to answer. Reading a project back
        // must not depend on which of the paths below wrote it.
        struct CodePage {
            int number;
            const char *canonical;
            const char *aliases; // lower case, separated by spaces
        };

        constexpr CodePage codePages[] = {
            {932,   "Shift_JIS",    "shift_jis shift-jis sjis ms_kanji cp932 windows-932"},
            // GB2312 is its own code page, and everything maps it to 936 because GBK holds all
            // of it and more, so reading one as the other cannot lose anything.
            {936,   "GBK",          "gbk gb2312 euc-cn cp936 windows-936"                },
            {950,   "Big5",         "big5 big-5 cp950 windows-950"                       },
            {949,   "EUC-KR",       "euc-kr ks_c_5601-1987 cp949 windows-949"            },
            {54936, "GB18030",      "gb18030"                                            },
            // The rest of the ANSI code pages, so that a machine set to any of them has its
            // system encoding here as well. What a UST from there is in, when it says nothing.
            {874,   "windows-874",  "windows-874 cp874"                                  },
            {1250,  "windows-1250", "windows-1250 cp1250"                                },
            {1251,  "windows-1251", "windows-1251 cp1251"                                },
            {1252,  "windows-1252", "windows-1252 cp1252"                                },
            {1253,  "windows-1253", "windows-1253 cp1253"                                },
            {1254,  "windows-1254", "windows-1254 cp1254"                                },
            {1255,  "windows-1255", "windows-1255 cp1255"                                },
            {1256,  "windows-1256", "windows-1256 cp1256"                                },
            {1257,  "windows-1257", "windows-1257 cp1257"                                },
            {1258,  "windows-1258", "windows-1258 cp1258"                                },
        };

        const CodePage *findCodePage(const QString &name) {
            const QString wanted = name.toLower();
            for (const auto &page : codePages) {
                for (const auto &alias : QString::fromLatin1(page.aliases)
                                             .split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
                    if (wanted == alias) {
                        return &page;
                    }
                }
            }
            return nullptr;
        }

    }

    // Four ways of doing the same job, tried in this order.
    //
    // 1. Qt's own converters, for the Unicode family and Latin-1. Built into QtCore.
    // 2. winacp, for the Windows ANSI code pages: Shift_JIS, GBK, Big5, EUC-KR and the rest, on
    //    every system.
    // 3. The Windows code page functions, for GB18030, on Windows.
    // 4. Qt by name, which reaches ICU where Qt has it, for anything else.
    //
    // UTAU reads and writes by the Windows code page, so that is the mapping a UST or an oto.ini
    // has to go through to come out as it went in, and winacp is that mapping, taken from
    // Windows and carried to the other systems. Nothing else is. Qt reaches ICU only where it
    // was built with it, and the Qt for macOS is not: there it could not open a single Shift_JIS
    // file. What macOS has of its own, CoreFoundation and iconv, leaves out the thousands of
    // characters Windows puts in the private use area, and writes some characters back as other
    // bytes than Windows does. The same way on every system also takes Windows' own ICU out of
    // it, which Qt loads at run time and which older Windows does not have.
    class TextCodec::Impl {
    public:
        explicit Impl(const QString &requested) {
            const QString name = requested.isEmpty() ? TextCodec::systemName() : requested;

            if (const auto found = QStringConverter::encodingForName(name)) {
                builtin = *found;
                canonical = QString::fromLatin1(QStringConverter::nameForEncoding(*found));
                valid = true;
                return;
            }

            if (const auto *page = findCodePage(name)) {
                canonical = QLatin1String(page->canonical);
                if (winacp::isAvailable(page->number)) {
                    ansiCodePage = page->number;
                    valid = true;
                    return;
                }
#ifdef _WIN32
                if (::IsValidCodePage(UINT(page->number))) {
                    codePage = page->number;
                    valid = true;
                    return;
                }
#endif
            }

            // Whatever is left, if ICU happens to be there.
            QStringDecoder byName(name);
            if (byName.isValid()) {
                fallbackName = name;
                if (canonical.isEmpty()) {
                    canonical = QString::fromLatin1(byName.name());
                }
                valid = true;
            }
        }

        QString canonical;
        bool valid = false;

        std::optional<QStringConverter::Encoding> builtin;
        int ansiCodePage = 0; // held by winacp
        QString fallbackName;
#ifdef _WIN32
        int codePage = 0;
#endif
    };

    TextCodec::TextCodec(const QString &name) : _impl(std::make_unique<Impl>(name)) {
    }

    TextCodec::~TextCodec() = default;

    TextCodec::TextCodec(const TextCodec &RHS) : _impl(std::make_unique<Impl>(*RHS._impl)) {
    }

    TextCodec &TextCodec::operator=(const TextCodec &RHS) {
        if (this != &RHS) {
            *_impl = *RHS._impl;
        }
        return *this;
    }

    bool TextCodec::isValid() const {
        return _impl->valid;
    }

    QString TextCodec::name() const {
        return _impl->valid ? _impl->canonical : QString();
    }

    bool TextCodec::isUtf8() const {
        return _impl->valid && _impl->builtin == QStringConverter::Utf8;
    }

    std::optional<QString> TextCodec::decode(QByteArrayView bytes) const {
        if (!_impl->valid) {
            return std::nullopt;
        }
        if (bytes.isEmpty()) {
            return QString();
        }

        if (_impl->builtin) {
            QStringDecoder decoder(*_impl->builtin);
            QString text = decoder.decode(bytes);
            return decoder.hasError() ? std::nullopt : std::optional<QString>(text);
        }

        if (_impl->ansiCodePage != 0) {
            // All or nothing: bytes that do not decode mean the wrong encoding was chosen.
            const auto text = winacp::decode(_impl->ansiCodePage,
                                             std::string_view(bytes.data(), size_t(bytes.size())));
            if (!text) {
                return std::nullopt;
            }
            return QString::fromUtf16(text->data(), qsizetype(text->size()));
        }

#ifdef _WIN32
        if (_impl->codePage != 0) {
            // MB_ERR_INVALID_CHARS is what makes this refuse rather than substitute, which is
            // the whole point: bytes that do not decode mean the wrong encoding was chosen.
            const int length = ::MultiByteToWideChar(UINT(_impl->codePage), MB_ERR_INVALID_CHARS,
                                                     bytes.data(), int(bytes.size()), nullptr, 0);
            if (length <= 0) {
                return std::nullopt;
            }
            QString text(length, Qt::Uninitialized);
            ::MultiByteToWideChar(UINT(_impl->codePage), MB_ERR_INVALID_CHARS, bytes.data(),
                                  int(bytes.size()), reinterpret_cast<wchar_t *>(text.data()),
                                  length);
            return text;
        }
#endif

        QStringDecoder decoder(_impl->fallbackName);
        QString text = decoder.decode(bytes);
        return decoder.hasError() ? std::nullopt : std::optional<QString>(text);
    }

    QByteArray TextCodec::encode(QStringView text) const {
        if (!_impl->valid || text.isEmpty()) {
            return {};
        }

        if (_impl->builtin) {
            QStringEncoder encoder(*_impl->builtin);
            return encoder.encode(text);
        }

        if (_impl->ansiCodePage != 0) {
            // A question mark for what the page cannot hold, as Windows writes, which is what
            // canEncode() compares against.
            const std::string written =
                winacp::encode(_impl->ansiCodePage,
                               std::u16string_view(reinterpret_cast<const char16_t *>(text.utf16()),
                                                   size_t(text.size())),
                               '?');
            return QByteArray(written.data(), qsizetype(written.size()));
        }

#ifdef _WIN32
        if (_impl->codePage != 0) {
            const auto *wide = reinterpret_cast<const wchar_t *>(text.utf16());
            const int size = ::WideCharToMultiByte(UINT(_impl->codePage), 0, wide, int(text.size()),
                                                   nullptr, 0, nullptr, nullptr);
            if (size <= 0) {
                return {};
            }
            QByteArray bytes(size, Qt::Uninitialized);
            ::WideCharToMultiByte(UINT(_impl->codePage), 0, wide, int(text.size()), bytes.data(),
                                  size, nullptr, nullptr);
            return bytes;
        }
#endif

        QStringEncoder encoder(_impl->fallbackName);
        return encoder.encode(text);
    }

    bool TextCodec::canEncode(QStringView text) const {
        if (!_impl->valid) {
            return false;
        }
        if (text.isEmpty()) {
            return true;
        }

        // Written out, read back and compared, on every path. An encoding that cannot hold a
        // character writes a question mark for it and says nothing, so only the comparison
        // catches that, and the comparison is also what keeps a text that was already a question
        // mark from looking like a failure.
        const QByteArray bytes = encode(text);
        if (bytes.isEmpty()) {
            return false;
        }
        const auto back = decode(bytes);
        return back && *back == text;
    }

    QString TextCodec::systemName() {
#ifdef _WIN32
        // What UTAU writes when it writes nothing about the encoding. Named rather than left as
        // QStringConverter::System, whose own name is the word "Locale" and cannot be recorded.
        const UINT acp = ::GetACP();
        if (acp == 65001) {
            return QStringLiteral("UTF-8");
        }
        for (const auto &page : codePages) {
            if (UINT(page.number) == acp) {
                return QLatin1String(page.canonical);
            }
        }
        return QStringLiteral("windows-%1").arg(acp);
#else
        // No such thing outside Windows. A file written there and carrying no declaration is
        // UTF-8 in every setting anyone still runs.
        return QStringLiteral("UTF-8");
#endif
    }

    QStringList TextCodec::ustCandidates() {
        QStringList names{
            QStringLiteral("UTF-8"),
            QStringLiteral("Shift_JIS"), // Japan
            QStringLiteral("GBK"),       // mainland China
            QStringLiteral("Big5"),      // Taiwan
        };
        const QString system = systemName();
        if (!names.contains(system, Qt::CaseInsensitive)) {
            names.append(system);
        }
        return names;
    }

    QStringList TextCodec::availableNames() {
        QStringList names;
        for (const auto &page : codePages) {
            names.append(QLatin1String(page.canonical));
        }
        for (const auto &name : QStringConverter::availableCodecs()) {
            if (!names.contains(name, Qt::CaseInsensitive)) {
                names.append(name);
            }
        }
        return names;
    }

    namespace {

        void appendEscape(QString &out, char16_t unit) {
            out += QStringLiteral("\\u%1").arg(uint(unit), 4, 16, QLatin1Char('0'));
        }

        bool isHexDigit(QChar c) {
            return (c >= QLatin1Char('0') && c <= QLatin1Char('9')) ||
                   (c >= QLatin1Char('a') && c <= QLatin1Char('f')) ||
                   (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
        }

    }

    QString TextCodec::escape(const QString &text) const {
        QString out;
        out.reserve(text.size());

        for (qsizetype i = 0; i < text.size(); ++i) {
            const QChar c = text.at(i);

            if (c == QLatin1Char('\\')) {
                out += QLatin1String("\\\\");
                continue;
            }

            // A character outside the basic plane is two code units that mean nothing apart, so
            // the pair is tested and written together.
            const qsizetype width =
                (c.isHighSurrogate() && i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) ? 2
                                                                                                : 1;
            const QStringView unit(text.constData() + i, width);

            if (canEncode(unit)) {
                out += unit;
            } else {
                for (qsizetype k = 0; k < width; ++k) {
                    appendEscape(out, unit.at(k).unicode());
                }
            }
            i += width - 1;
        }

        return out;
    }

    QString TextCodec::unescape(const QString &text) {
        QString out;
        out.reserve(text.size());

        for (qsizetype i = 0; i < text.size(); ++i) {
            const QChar c = text.at(i);
            if (c != QLatin1Char('\\') || i + 1 >= text.size()) {
                out += c;
                continue;
            }

            const QChar next = text.at(i + 1);
            if (next == QLatin1Char('\\')) {
                out += QLatin1Char('\\');
                ++i;
                continue;
            }

            if (next == QLatin1Char('u') && i + 5 < text.size() && isHexDigit(text.at(i + 2)) &&
                isHexDigit(text.at(i + 3)) && isHexDigit(text.at(i + 4)) &&
                isHexDigit(text.at(i + 5))) {
                bool ok = false;
                const auto unit = QStringView(text).mid(i + 2, 4).toUShort(&ok, 16);
                if (ok) {
                    out += QChar(char16_t(unit));
                    i += 5;
                    continue;
                }
            }

            // Not an escape this understands. Kept as it stands, because it came from somewhere
            // and dropping it would lose whatever it was.
            out += c;
        }

        return out;
    }

}
