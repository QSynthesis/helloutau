#include "TextSearch.h"

namespace hello::kit {

    TextSearch::TextSearch() = default;

    TextSearch::TextSearch(const QString &pattern, Options options)
        : m_pattern(pattern), m_options(options) {
        if (pattern.isEmpty()) {
            return;
        }
        // Unicode properties give \w and \b the word characters of every script, which whole
        // words require for kana and hanzi.
        QRegularExpression::PatternOptions flags = QRegularExpression::UseUnicodePropertiesOption;
        if (!(options & CaseSensitive)) {
            flags |= QRegularExpression::CaseInsensitiveOption;
        }
        auto source = options & RegularExpression ? pattern : QRegularExpression::escape(pattern);
        if (options & RegularExpression) {
            // The pattern is checked alone, because the enclosing group below can balance an
            // unbalanced parenthesis of the pattern.
            const QRegularExpression alone(source, flags);
            if (!alone.isValid()) {
                m_error = alone.errorString();
                return;
            }
        }
        if (options & WholeWord) {
            source = QStringLiteral("(?<!\\w)(?:%1)(?!\\w)").arg(source);
        }
        m_expression = QRegularExpression(source, flags);
        if (!m_expression.isValid()) {
            m_error = m_expression.errorString();
        }
    }

    QString TextSearch::pattern() const {
        return m_pattern;
    }

    TextSearch::Options TextSearch::options() const {
        return m_options;
    }

    bool TextSearch::isValid() const {
        return !m_pattern.isEmpty() && m_error.isEmpty();
    }

    QString TextSearch::errorString() const {
        return m_error;
    }

    bool TextSearch::matches(const QString &text) const {
        return isValid() && m_expression.match(text).hasMatch();
    }

    QList<TextSearch::Match> TextSearch::matchesIn(const QString &text) const {
        QList<Match> matches;
        if (!isValid()) {
            return matches;
        }
        auto it = m_expression.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            matches.push_back({match.capturedStart(), match.capturedLength()});
        }
        return matches;
    }

    QString TextSearch::replaced(const QString &text, const QString &replacement) const {
        if (!isValid()) {
            return text;
        }
        QString result;
        qsizetype last = 0;
        auto it = m_expression.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            result += QStringView(text).sliced(last, match.capturedStart() - last);
            result += m_options & RegularExpression ? expanded(match, replacement) : replacement;
            last = match.capturedEnd();
        }
        result += QStringView(text).sliced(last);
        return result;
    }

    QString TextSearch::expanded(const QRegularExpressionMatch &match,
                                 const QString &replacement) const {
        QString result;
        const int groups = m_expression.captureCount();
        for (qsizetype i = 0; i < replacement.size(); ++i) {
            const auto c = replacement.at(i);
            if (c != u'$' || i + 1 >= replacement.size()) {
                result += c;
                continue;
            }
            const auto next = replacement.at(i + 1);
            if (next == u'$') {
                result += u'$';
                ++i;
                continue;
            }
            if (next < u'0' || next > u'9') {
                result += c;
                continue;
            }
            // The reference has two digits if they denote a group, as $10 does in an expression
            // of ten groups, and one digit otherwise.
            int number = next.unicode() - u'0';
            qsizetype used = 1;
            if (i + 2 < replacement.size()) {
                const auto second = replacement.at(i + 2);
                if (second >= u'0' && second <= u'9') {
                    const int two = number * 10 + (second.unicode() - u'0');
                    if (two <= groups) {
                        number = two;
                        used = 2;
                    }
                }
            }
            if (number > groups) {
                // A number beyond the groups of the expression is copied as written.
                result += c;
                continue;
            }
            result += match.captured(number);
            i += used;
        }
        return result;
    }

}
