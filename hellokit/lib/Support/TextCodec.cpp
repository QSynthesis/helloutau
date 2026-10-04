#include "TextCodec.h"

#ifdef _WIN32
#  include <QtCore/qt_windows.h>
#endif

#include <algorithm>
#include <limits>

#include <QtCore/QStringConverter>
#include <QtCore/QStringDecoder>
#include <QtCore/QStringEncoder>

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
    TextCodec::TextCodec(const QString &name) {
        const QString requested = name.isEmpty() ? systemName() : name;

        if (const auto found = QStringConverter::encodingForName(requested)) {
            m_path = Path::Builtin;
            m_builtin = *found;
            m_name = QString::fromLatin1(QStringConverter::nameForEncoding(*found));
            return;
        }

        QString canonical;
        if (const auto page = findCodePage(requested)) {
            canonical = QLatin1String(page->canonical);
            if (winacp::codePageFromNumber(page->number)) {
                m_path = Path::AnsiCodePage;
                m_codePage = page->number;
                m_name = canonical;
                return;
            }
#ifdef _WIN32
            if (::IsValidCodePage(UINT(page->number))) {
                m_path = Path::WindowsCodePage;
                m_codePage = page->number;
                m_name = canonical;
                return;
            }
#endif
        }

        // All other encodings, if ICU is available.
        QStringDecoder byName(requested);
        if (byName.isValid()) {
            m_path = Path::ByName;
            m_converterName = requested;
            m_name = canonical.isEmpty() ? QString::fromLatin1(byName.name()) : canonical;
        }
    }

    bool TextCodec::isValid() const {
        return m_path != Path::Invalid;
    }

    QString TextCodec::name() const {
        return m_name;
    }

    bool TextCodec::isUtf8() const {
        return m_path == Path::Builtin && m_builtin == QStringConverter::Utf8;
    }

    std::optional<QString> TextCodec::decode(QByteArrayView bytes) const {
        if (m_path == Path::Invalid) {
            return std::nullopt;
        }
        if (bytes.isEmpty()) {
            return QString();
        }

        switch (m_path) {
            // Stateless, so that a sequence cut off at the end is an error rather than state kept
            // for a next call that never comes.
            case Path::Builtin: {
                QStringDecoder decoder(m_builtin, QStringConverter::Flag::Stateless);
                QString text = decoder.decode(bytes);
                return decoder.hasError() ? std::nullopt : std::optional<QString>(text);
            }

            case Path::AnsiCodePage: {
                // All or nothing, because invalid bytes indicate an incorrect encoding choice.
                const auto text =
                    winacp::decode(winacp::CodePage(m_codePage),
                                   std::string_view(bytes.data(), size_t(bytes.size())));
                if (!text) {
                    return std::nullopt;
                }
                return QString::fromUtf16(text->data(), qsizetype(text->size()));
            }

#ifdef _WIN32
            case Path::WindowsCodePage: {
                // MB_ERR_INVALID_CHARS makes the conversion fail instead of substituting, which is
                // required because invalid bytes indicate an incorrect encoding choice.
                const int length =
                    ::MultiByteToWideChar(UINT(m_codePage), MB_ERR_INVALID_CHARS, bytes.data(),
                                          int(bytes.size()), nullptr, 0);
                if (length <= 0) {
                    return std::nullopt;
                }
                QString text(length, Qt::Uninitialized);
                ::MultiByteToWideChar(UINT(m_codePage), MB_ERR_INVALID_CHARS, bytes.data(),
                                      int(bytes.size()), reinterpret_cast<wchar_t *>(text.data()),
                                      length);
                return text;
            }
#endif

            case Path::ByName: {
                QStringDecoder decoder(m_converterName, QStringConverter::Flag::Stateless);
                QString text = decoder.decode(bytes);
                return decoder.hasError() ? std::nullopt : std::optional<QString>(text);
            }

            default:
                break;
        }
        return std::nullopt;
    }

    QString TextCodec::decodeReplacing(QByteArrayView bytes, qsizetype *invalid) const {
        qsizetype count = 0;
        QString text;
        if (const auto whole = decode(bytes)) {
            text = *whole;
        } else if (m_path != Path::Invalid) {
            // Each position is decoded as the shortest sequence that is valid by itself. Four
            // bytes is the longest character of every supported encoding, reached by UTF-8 and
            // GB18030.
            qsizetype at = 0;
            while (at < bytes.size()) {
                qsizetype length = 1;
                std::optional<QString> piece;
                for (; length <= 4 && at + length <= bytes.size(); ++length) {
                    piece = decode(bytes.sliced(at, length));
                    if (piece) {
                        break;
                    }
                }
                if (piece) {
                    text += *piece;
                    at += length;
                } else {
                    text += QChar::ReplacementCharacter;
                    ++count;
                    ++at;
                }
            }
        }
        if (invalid) {
            *invalid = count;
        }
        return text;
    }

    QByteArray TextCodec::encode(QStringView text) const {
        if (text.isEmpty()) {
            return {};
        }

        switch (m_path) {
            case Path::Builtin: {
                QStringEncoder encoder(m_builtin);
                return encoder.encode(text);
            }

            case Path::AnsiCodePage: {
                // A question mark for each unrepresentable code unit, as Windows writes.
                // canEncode() detects the substitution by comparison.
                const std::string written = winacp::encode(
                    winacp::CodePage(m_codePage),
                    std::u16string_view(reinterpret_cast<const char16_t *>(text.utf16()),
                                        size_t(text.size())),
                    '?');
                return QByteArray(written.data(), qsizetype(written.size()));
            }

#ifdef _WIN32
            case Path::WindowsCodePage: {
                const auto wide = reinterpret_cast<const wchar_t *>(text.utf16());
                const int size = ::WideCharToMultiByte(UINT(m_codePage), 0, wide, int(text.size()),
                                                       nullptr, 0, nullptr, nullptr);
                if (size <= 0) {
                    return {};
                }
                QByteArray bytes(size, Qt::Uninitialized);
                ::WideCharToMultiByte(UINT(m_codePage), 0, wide, int(text.size()), bytes.data(),
                                      size, nullptr, nullptr);
                return bytes;
            }
#endif

            case Path::ByName: {
                QStringEncoder encoder(m_converterName);
                return encoder.encode(text);
            }

            default:
                break;
        }
        return {};
    }

    bool TextCodec::canEncode(QStringView text) const {
        if (m_path == Path::Invalid) {
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

    qint64 TextCodec::plausibility(const QList<QByteArray> &texts, const QString &name) {
        const TextCodec codec(name);
        if (!codec.isValid()) {
            return std::numeric_limits<qint64>::min();
        }
        const bool utf8 = codec.isUtf8();
        qint64 score = 0;
        for (const auto &bytes : texts) {
            qsizetype invalid = 0;
            const auto text = codec.decodeReplacing(bytes, &invalid);
            score -= 10 * qint64(invalid);
            for (const auto character : text) {
                const char16_t c = character.unicode();
                if (c < 0x80) {
                    if (c < 0x20 && c != u'\t' && c != u'\n' && c != u'\r') {
                        score -= 10;
                    }
                    continue;
                }
                if (utf8) {
                    score += 2;
                }
                if (c == 0xFFFD) {
                    continue; // counted as invalid
                } else if (c < 0xA0 || (c >= 0xE000 && c < 0xF900)) {
                    score -= 10; // C1 controls, the private use area
                } else if (c < 0x250) {
                    score -= 1; // Latin-1 and Latin Extended, as bytes of another text read
                } else if (c >= 0x3040 && c < 0x3100) {
                    score += 2; // kana
                } else if ((c >= 0x4E00 && c < 0xA000) || (c >= 0x3400 && c < 0x4DC0) ||
                           (c >= 0xAC00 && c < 0xD7A4) || (c >= 0x3000 && c < 0x3040) ||
                           (c >= 0xFF01 && c < 0xFF5F)) {
                    score += 1; // ideographs, Hangul, CJK and full-width punctuation
                } else if (c >= 0xFF61 && c < 0xFFA0) {
                    score -= 3; // half-width katakana
                }
            }
        }
        return score;
    }

    QStringList TextCodec::ranked(const QList<QByteArray> &texts, const QStringList &candidates) {
        QList<std::pair<qint64, QString>> scored;
        for (const auto &name : candidates) {
            scored.push_back({plausibility(texts, name), name});
        }
        std::stable_sort(scored.begin(), scored.end(),
                         [](const auto &a, const auto &b) { return a.first > b.first; });
        QStringList names;
        for (const auto &[score, name] : scored) {
            names.push_back(name);
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
