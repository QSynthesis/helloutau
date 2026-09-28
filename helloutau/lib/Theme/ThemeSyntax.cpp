#include "ThemeSyntax.h"

namespace hello::daw {

    namespace {

        bool isWordCharacter(QChar c) {
            return !c.isSpace() && c != u',' && c != u'(' && c != u')' && c != u'=' && c != u'"' &&
                   c != u'\'';
        }

        // Reads the syntax by recursive descent. The first error ends the reading.
        class Reader {
        public:
            explicit Reader(QStringView text) : m_text(text) {
            }

            std::optional<ThemeValue> value() {
                skipBlanks();
                auto first = primary();
                if (!first) {
                    return std::nullopt;
                }
                ThemeValue sequence;
                sequence.kind = ThemeValue::Sequence;
                sequence.position = first->position;
                sequence.items.push_back(std::move(*first));
                while (true) {
                    skipBlanks();
                    if (atEnd() || peek() == u',' || peek() == u')' || peek() == u'=') {
                        break;
                    }
                    auto next = primary();
                    if (!next) {
                        return std::nullopt;
                    }
                    sequence.items.push_back(std::move(*next));
                }
                if (sequence.items.size() == 1) {
                    return std::move(sequence.items.front());
                }
                return sequence;
            }

            // The arguments up to \a close, or to the end of the text if \a close is null
            std::optional<std::vector<ThemeArgument>> arguments(QChar close) {
                std::vector<ThemeArgument> result;
                skipBlanks();
                if (closes(close)) {
                    return result;
                }
                bool keywords = false;
                while (true) {
                    skipBlanks();
                    const qsizetype start = m_at;
                    auto first = value();
                    if (!first) {
                        return std::nullopt;
                    }
                    skipBlanks();
                    ThemeArgument argument;
                    if (!atEnd() && peek() == u'=') {
                        if (first->kind != ThemeValue::Word) {
                            return fail(start, ThemeSyntax::tr("A keyword must be a word."));
                        }
                        ++m_at;
                        argument.key = first->text;
                        auto second = value();
                        if (!second) {
                            return std::nullopt;
                        }
                        argument.value = std::move(*second);
                        keywords = true;
                    } else {
                        if (keywords) {
                            return fail(start, ThemeSyntax::tr("A positional argument cannot "
                                                               "follow a keyword argument."));
                        }
                        argument.value = std::move(*first);
                    }
                    result.push_back(std::move(argument));
                    skipBlanks();
                    if (!atEnd() && peek() == u',') {
                        ++m_at;
                        continue;
                    }
                    if (closes(close)) {
                        return result;
                    }
                    return fail(
                        m_at, close.isNull()
                                  ? ThemeSyntax::tr("A comma or the end was expected.")
                                  : ThemeSyntax::tr("A comma or \"%1\" was expected.").arg(close));
                }
            }

            bool atEnd() const {
                return m_at >= m_text.size();
            }

            qsizetype at() const {
                return m_at;
            }

            void skipBlanks() {
                while (!atEnd()) {
                    if (peek().isSpace()) {
                        ++m_at;
                    } else if (m_text.sliced(m_at).startsWith(u"/*")) {
                        const auto end = m_text.indexOf(u"*/", m_at + 2);
                        m_at = end < 0 ? m_text.size() : end + 2;
                    } else {
                        break;
                    }
                }
            }

            ThemeError error;

            template <class T = ThemeValue>
            std::nullopt_t fail(qsizetype position, const QString &message) {
                error = {position, message};
                return std::nullopt;
            }

        private:
            QStringView m_text;
            qsizetype m_at = 0;

            QChar peek() const {
                return m_text[m_at];
            }

            bool closes(QChar close) const {
                return close.isNull() ? atEnd() : !atEnd() && peek() == close;
            }

            std::optional<ThemeValue> primary() {
                if (atEnd()) {
                    return fail(m_at, ThemeSyntax::tr("A value was expected."));
                }
                const qsizetype start = m_at;
                const QChar c = peek();
                if (c == u'"' || c == u'\'') {
                    return string(c);
                }
                if (c == u'(') {
                    ++m_at;
                    auto group = arguments(u')');
                    if (!group) {
                        return std::nullopt;
                    }
                    ++m_at;
                    ThemeValue value;
                    value.kind = ThemeValue::Group;
                    value.arguments = std::move(*group);
                    value.position = start;
                    return value;
                }
                if (!isWordCharacter(c)) {
                    return fail(start, ThemeSyntax::tr("\"%1\" is not expected here.").arg(c));
                }
                while (!atEnd() && isWordCharacter(peek())) {
                    ++m_at;
                }
                ThemeValue value;
                value.text = m_text.sliced(start, m_at - start).toString();
                value.position = start;
                if (!atEnd() && peek() == u'(') {
                    ++m_at;
                    auto list = arguments(u')');
                    if (!list) {
                        return std::nullopt;
                    }
                    ++m_at;
                    value.kind = ThemeValue::Function;
                    value.arguments = std::move(*list);
                }
                return value;
            }

            std::optional<ThemeValue> string(QChar quote) {
                const qsizetype start = m_at++;
                QString text;
                while (!atEnd() && peek() != quote) {
                    if (peek() == u'\\' && m_at + 1 < m_text.size()) {
                        ++m_at;
                    }
                    text.append(peek());
                    ++m_at;
                }
                if (atEnd()) {
                    return fail(start, ThemeSyntax::tr("The string is not closed."));
                }
                ++m_at;
                ThemeValue value;
                value.kind = ThemeValue::String;
                value.text = text;
                value.position = start;
                return value;
            }
        };

        QString argumentsText(const std::vector<ThemeArgument> &arguments) {
            QStringList parts;
            for (const auto &argument : arguments) {
                parts.push_back(argument.key.isEmpty()
                                    ? argument.value.toString()
                                    : argument.key + u'=' + argument.value.toString());
            }
            return parts.join(QStringLiteral(", "));
        }

    }

    QString ThemeValue::toString() const {
        switch (kind) {
            case Word:
                return text;
            case String: {
                QString escaped = text;
                escaped.replace(u'\\', QStringLiteral("\\\\"))
                    .replace(u'"', QStringLiteral("\\\""));
                return u'"' + escaped + u'"';
            }
            case Function:
                return text + u'(' + argumentsText(arguments) + u')';
            case Group:
                return u'(' + argumentsText(arguments) + u')';
            case Sequence: {
                QStringList parts;
                for (const auto &item : items) {
                    parts.push_back(item.toString());
                }
                return parts.join(u' ');
            }
        }
        return {};
    }

    std::optional<ThemeValue> ThemeSyntax::parse(QStringView text, ThemeError *error) {
        Reader reader(text);
        auto value = reader.value();
        if (value) {
            reader.skipBlanks();
            if (!reader.atEnd()) {
                value.reset();
                reader.error = {reader.at(), tr("Nothing may follow the value.")};
            }
        }
        if (!value && error) {
            *error = reader.error;
        }
        return value;
    }

    std::optional<std::vector<ThemeArgument>> ThemeSyntax::parseArguments(QStringView text,
                                                                          ThemeError *error) {
        Reader reader(text);
        auto arguments = reader.arguments(QChar());
        if (!arguments && error) {
            *error = reader.error;
        }
        return arguments;
    }

}
