#include "TextCodec.h"

#ifdef _WIN32
#  include <QtCore/qt_windows.h>
#endif

#include <QtCore/QStringConverter>
#include <QtCore/QStringDecoder>
#include <QtCore/QStringEncoder>

#include <stdcorelib/pimpl.h>
#include <winacp/winacp.h>

namespace hello::kit {

    namespace {

        // The encodings supported by this project, keyed by the corresponding Windows code page.
        //
        // The canonical name is recorded in the control note, so it is defined here rather than
        // taken from whichever library performs the conversion. Reading a project must not
        // depend on the conversion path used to write it.
        struct CodePage {
            int number;
            const char *canonical;
            const char *aliases; // lower case, separated by spaces
        };

        constexpr CodePage codePages[] = {
            {932,   "Shift_JIS",    "shift_jis shift-jis sjis ms_kanji cp932 windows-932"},
            // GB2312 is a separate code page, but it is universally mapped to 936, because GBK is
            // a superset of GB2312 and reading one as the other loses nothing.
            {936,   "GBK",          "gbk gb2312 euc-cn cp936 windows-936"                },
            {950,   "Big5",         "big5 big-5 cp950 windows-950"                       },
            {949,   "EUC-KR",       "euc-kr ks_c_5601-1987 cp949 windows-949"            },
            {54936, "GB18030",      "gb18030"                                            },
            // The remaining ANSI code pages, so that the system encoding of any Windows machine
            // is available. A UST from such a machine without an encoding declaration uses it.
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

    // Four conversion paths, tried in this order:
    //
    // 1. The built-in converters of Qt, for the Unicode encodings and Latin-1.
    // 2. winacp, for the Windows ANSI code pages, including Shift_JIS, GBK, Big5 and EUC-KR, on
    //    every system.
    // 3. The Windows code page functions, for GB18030, on Windows only.
    // 4. Qt by name, which uses ICU if Qt was built with it, for all other encodings.
    //
    // UTAU reads and writes files in the Windows code page, so a UST or an oto.ini must be
    // converted with exactly that mapping to round-trip unchanged. winacp provides that mapping,
    // captured from Windows, on every system, and no other library does. Qt uses ICU only if
    // built with it, and the Qt distribution for macOS is not, so it could not open any
    // Shift_JIS file there. The native macOS converters, Core Foundation and iconv, omit the
    // several thousand characters that Windows maps into the Private Use Area, and encode some
    // characters to byte sequences other than those Windows produces. A uniform path on every
    // system also removes the dependency on the ICU of Windows, which Qt loads at run time and
    // which older Windows versions lack.
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
                if (const auto supported = winacp::codePageFromNumber(page->number)) {
                    ansiCodePage = *supported;
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

            // All other encodings, if ICU is available.
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
        std::optional<winacp::CodePage> ansiCodePage; // held by winacp
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
            stdc_impl_t;
            impl = *RHS._impl;
        }
        return *this;
    }

    bool TextCodec::isValid() const {
        stdc_impl_t;
        return impl.valid;
    }

    QString TextCodec::name() const {
        stdc_impl_t;
        return impl.valid ? impl.canonical : QString();
    }

    bool TextCodec::isUtf8() const {
        stdc_impl_t;
        return impl.valid && impl.builtin == QStringConverter::Utf8;
    }

    std::optional<QString> TextCodec::decode(QByteArrayView bytes) const {
        stdc_impl_t;
        if (!impl.valid) {
            return std::nullopt;
        }
        if (bytes.isEmpty()) {
            return QString();
        }

        if (impl.builtin) {
            QStringDecoder decoder(*impl.builtin);
            QString text = decoder.decode(bytes);
            return decoder.hasError() ? std::nullopt : std::optional<QString>(text);
        }

        if (impl.ansiCodePage) {
            // All or nothing, because invalid bytes indicate an incorrect encoding choice.
            const auto text = winacp::decode(*impl.ansiCodePage,
                                             std::string_view(bytes.data(), size_t(bytes.size())));
            if (!text) {
                return std::nullopt;
            }
            return QString::fromUtf16(text->data(), qsizetype(text->size()));
        }

#ifdef _WIN32
        if (impl.codePage != 0) {
            // MB_ERR_INVALID_CHARS makes the conversion fail instead of substituting, which is
            // required because invalid bytes indicate an incorrect encoding choice.
            const int length = ::MultiByteToWideChar(UINT(impl.codePage), MB_ERR_INVALID_CHARS,
                                                     bytes.data(), int(bytes.size()), nullptr, 0);
            if (length <= 0) {
                return std::nullopt;
            }
            QString text(length, Qt::Uninitialized);
            ::MultiByteToWideChar(UINT(impl.codePage), MB_ERR_INVALID_CHARS, bytes.data(),
                                  int(bytes.size()), reinterpret_cast<wchar_t *>(text.data()),
                                  length);
            return text;
        }
#endif

        QStringDecoder decoder(impl.fallbackName);
        QString text = decoder.decode(bytes);
        return decoder.hasError() ? std::nullopt : std::optional<QString>(text);
    }

    QByteArray TextCodec::encode(QStringView text) const {
        stdc_impl_t;
        if (!impl.valid || text.isEmpty()) {
            return {};
        }

        if (impl.builtin) {
            QStringEncoder encoder(*impl.builtin);
            return encoder.encode(text);
        }

        if (impl.ansiCodePage) {
            // A question mark for each unrepresentable code unit, as Windows writes. canEncode()
            // detects the substitution by comparison.
            const std::string written =
                winacp::encode(*impl.ansiCodePage,
                               std::u16string_view(reinterpret_cast<const char16_t *>(text.utf16()),
                                                   size_t(text.size())),
                               '?');
            return QByteArray(written.data(), qsizetype(written.size()));
        }

#ifdef _WIN32
        if (impl.codePage != 0) {
            const auto *wide = reinterpret_cast<const wchar_t *>(text.utf16());
            const int size = ::WideCharToMultiByte(UINT(impl.codePage), 0, wide, int(text.size()),
                                                   nullptr, 0, nullptr, nullptr);
            if (size <= 0) {
                return {};
            }
            QByteArray bytes(size, Qt::Uninitialized);
            ::WideCharToMultiByte(UINT(impl.codePage), 0, wide, int(text.size()), bytes.data(),
                                  size, nullptr, nullptr);
            return bytes;
        }
#endif

        QStringEncoder encoder(impl.fallbackName);
        return encoder.encode(text);
    }

    bool TextCodec::canEncode(QStringView text) const {
        stdc_impl_t;
        if (!impl.valid) {
            return false;
        }
        if (text.isEmpty()) {
            return true;
        }

        // Encoded, decoded and compared, on every path. An encoder substitutes a question mark
        // for an unrepresentable character without reporting an error, so only the comparison
        // detects it. The comparison also prevents a literal question mark from being mistaken
        // for a failure.
        const QByteArray bytes = encode(text);
        if (bytes.isEmpty()) {
            return false;
        }
        const auto back = decode(bytes);
        return back && *back == text;
    }

    QString TextCodec::systemName() {
#ifdef _WIN32
        // The encoding UTAU uses for a file without an encoding declaration. Resolved to a name
        // rather than left as QStringConverter::System, whose name is "Locale" and cannot be
        // recorded.
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
        // Systems other than Windows have no ANSI code page. A file written there without a
        // declaration is UTF-8 in every configuration still in use.
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

            // A character outside the Basic Multilingual Plane is a surrogate pair, whose units
            // are meaningless in isolation, so the pair is tested and written together.
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

            // Not a recognized escape sequence. Preserved verbatim, because it is part of the
            // original text and dropping it would lose data.
            out += c;
        }

        return out;
    }

}
