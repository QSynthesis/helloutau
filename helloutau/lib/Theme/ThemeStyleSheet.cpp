#include "ThemeStyleSheet.h"

#include "ThemeIcon.h"
#include "ThemeLogging_p.h"

namespace hello::daw {

    namespace {

        bool isNameCharacter(QChar c) {
            return c.isLetterOrNumber() || c == u'_' || c == u'-';
        }

        // Scans a style sheet once, copying it to the output with the conversions applied.
        class Converter {
        public:
            Converter(QStringView text, const ThemeStyleSheet::Options &options)
                : m_text(text), m_options(options) {
            }

            QString run() {
                while (m_at < m_text.size()) {
                    const QChar c = m_text[m_at];
                    if (startsWith(u"/*")) {
                        copyUntilAfter(u"*/");
                    } else if (c == u'"' || c == u'\'') {
                        copyString(c);
                    } else if (c == u'{') {
                        ++m_depth;
                        beginDeclaration(c);
                    } else if (c == u'}') {
                        m_depth = std::max(0, m_depth - 1);
                        m_declarationStart = false;
                        copy(1);
                    } else if (m_depth > 0) {
                        declarationPart(c);
                    } else {
                        selectorPart();
                    }
                }
                return m_out;
            }

        private:
            QStringView m_text;
            const ThemeStyleSheet::Options &m_options;
            QString m_out;
            qsizetype m_at = 0;
            int m_depth = 0;
            bool m_declarationStart = false;
            QString m_property;

            bool startsWith(QStringView prefix) const {
                return m_text.sliced(m_at).startsWith(prefix);
            }

            void copy(qsizetype count) {
                m_out += m_text.sliced(m_at, count);
                m_at += count;
            }

            void copyUntilAfter(QStringView end) {
                const auto found = m_text.indexOf(end, m_at + end.size());
                copy(found < 0 ? m_text.size() - m_at : found + end.size() - m_at);
            }

            void copyString(QChar quote) {
                qsizetype end = m_at + 1;
                while (end < m_text.size() && m_text[end] != quote) {
                    end += m_text[end] == u'\\' ? 2 : 1;
                }
                copy(std::min(end + 1, m_text.size()) - m_at);
            }

            void beginDeclaration(QChar c) {
                m_out += c;
                ++m_at;
                m_declarationStart = true;
                m_property.clear();
            }

            void declarationPart(QChar c) {
                if (c == u';') {
                    beginDeclaration(c);
                    return;
                }
                if (m_declarationStart) {
                    if (c.isSpace()) {
                        copy(1);
                        return;
                    }
                    m_declarationStart = false;
                    qsizetype end = m_at;
                    while (end < m_text.size() && isNameCharacter(m_text[end])) {
                        ++end;
                    }
                    if (end == m_at) {
                        copy(1);
                        return;
                    }
                    QString name = m_text.sliced(m_at, end - m_at).toString();
                    if (name.startsWith(u"---")) {
                        name = name.mid(3);
                    } else if (name.startsWith(u"--")) {
                        name = QStringLiteral("qproperty-") + name.mid(2);
                    }
                    m_out += name;
                    m_property = name;
                    m_at = end;
                    return;
                }
                if (startsWith(u"svg(") && (m_at == 0 || !isNameCharacter(m_text[m_at - 1]))) {
                    icon();
                    return;
                }
                if (startsWith(u"url(@/")) {
                    m_out += QStringLiteral("url(") + m_options.directory + u'/';
                    m_at += 6;
                    return;
                }
                const bool startsNumber = c.isDigit() || (c == u'.' && m_at + 1 < m_text.size() &&
                                                          m_text[m_at + 1].isDigit());
                if (startsNumber &&
                    (m_at == 0 || !isNameCharacter(m_text[m_at - 1]) || m_text[m_at - 1] == u'-')) {
                    length();
                    return;
                }
                copy(1);
            }

            // Converts svg(...) into url("<name>.svgx"), which encodes a ThemeIcon, and resolves
            // its files written @/... against the directory of the style sheet. A malformed
            // svg(...) is reported and left unchanged, and Qt ignores it.
            void icon() {
                const qsizetype open = m_at + 4;
                const qsizetype close = closingParenthesis(open);
                if (close < 0) {
                    copy(1);
                    return;
                }
                const auto inner = m_text.sliced(open, close - open);
                ThemeError error;
                const auto arguments = ThemeSyntax::parseArguments(inner, &error);
                auto icon = arguments ? ThemeIcon::read(*arguments, &error) : std::nullopt;
                if (!icon) {
                    qCWarning(lcTheme).noquote() << QStringLiteral("svg(%1): %2 (at %3)")
                                                        .arg(inner.toString(), error.message)
                                                        .arg(error.position);
                    copy(close + 1 - m_at);
                    return;
                }
                for (size_t i = 0; i < 8; ++i) {
                    const auto state = ThemeButtonState(i);
                    const auto &file = icon->files.value(state);
                    if (file.startsWith(u"@/")) {
                        icon->files.setValue(state, m_options.directory + file.mid(1));
                    }
                }
                m_out += QStringLiteral("url(\"") + icon->fileName() + QStringLiteral("\")");
                m_at = close + 1;
            }

            // Returns the offset of the parenthesis that closes the parenthesis preceding \a at,
            // or -1 if the parenthesis is not closed
            qsizetype closingParenthesis(qsizetype at) const {
                int depth = 1;
                while (at < m_text.size()) {
                    const QChar c = m_text[at];
                    if (c == u'"' || c == u'\'') {
                        ++at;
                        while (at < m_text.size() && m_text[at] != c) {
                            at += m_text[at] == u'\\' ? 2 : 1;
                        }
                    } else if (c == u'(') {
                        ++depth;
                    } else if (c == u')' && --depth == 0) {
                        return at;
                    }
                    ++at;
                }
                return -1;
            }

            // Copies a number, scaled if the number is a length in pixels
            void length() {
                qsizetype end = m_at;
                while (end < m_text.size() && (m_text[end].isDigit() || m_text[end] == u'.')) {
                    ++end;
                }
                const auto number = m_text.sliced(m_at, end - m_at);
                const bool pixels = m_text.sliced(end).startsWith(u"px") &&
                                    (end + 2 >= m_text.size() || !isNameCharacter(m_text[end + 2]));
                const double scale =
                    m_property == u"font-size" ? m_options.fontScale : m_options.scale;
                if (!pixels || scale == 1) {
                    copy(end - m_at);
                    return;
                }
                m_out += QString::number(qRound(number.toDouble() * scale));
                m_at = end;
            }

            // Copies a selector, with :not(:x) converted to :!x
            void selectorPart() {
                if (startsWith(u":not(")) {
                    qsizetype at = m_at + 5;
                    while (at < m_text.size() && m_text[at].isSpace()) {
                        ++at;
                    }
                    if (at < m_text.size() && m_text[at] == u':') {
                        qsizetype end = at + 1;
                        while (end < m_text.size() && isNameCharacter(m_text[end])) {
                            ++end;
                        }
                        const auto name = m_text.sliced(at + 1, end - at - 1);
                        while (end < m_text.size() && m_text[end].isSpace()) {
                            ++end;
                        }
                        if (!name.isEmpty() && end < m_text.size() && m_text[end] == u')') {
                            m_out += QStringLiteral(":!") + name;
                            m_at = end + 1;
                            return;
                        }
                    }
                }
                copy(1);
            }
        };

    }

    QString ThemeStyleSheet::preprocess(QStringView text, const Options &options) {
        return Converter(text, options).run();
    }

}
