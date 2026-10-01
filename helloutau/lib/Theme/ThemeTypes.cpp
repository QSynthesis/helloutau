#include "ThemeTypes.h"

#include <mutex>

#include <QtCore/QLoggingCategory>

#include "ThemeArguments_p.h"
#include "ThemeLogging_p.h"
#include "ThemeReader.h"

namespace hello::daw {

    Q_LOGGING_CATEGORY(lcTheme, "hello.theme")

    namespace {

        // The texts are marked with QT_TRANSLATE_NOOP, since lupdate would attribute those of a
        // function named tr at namespace scope to the namespace or to the enclosing class.
        QString translated(const char *text) {
            return QCoreApplication::translate("hello::daw::ThemeTypes", text);
        }

        template <class T>
        std::optional<T> fail(ThemeError *error, qsizetype position, const QString &message) {
            if (error) {
                *error = {position, message};
            }
            return std::nullopt;
        }

        // Returns the value in \a choices whose name matches \a value
        template <class T>
        std::optional<T> choice(const ThemeValue &value,
                                std::initializer_list<std::pair<QStringView, T>> choices,
                                ThemeError *error) {
            for (const auto &[name, result] : choices) {
                if (value.kind == ThemeValue::Word && value.text == name) {
                    return result;
                }
            }
            QStringList names;
            for (const auto &[name, result] : choices) {
                names.push_back(name.toString());
            }
            return fail<T>(
                error, value.position,
                translated(QT_TRANSLATE_NOOP("hello::daw::ThemeTypes", "One of %1 was expected."))
                    .arg(names.join(QStringLiteral(", "))));
        }

        std::optional<ThemeStates<QColor>> colors(const ThemeValue &value, ThemeError *error) {
            return ThemeStates<QColor>::read(
                value, [](const ThemeValue &v, ThemeError *e) { return ThemeReader::color(v, e); },
                error);
        }

        // Returns the items of a group, or \a value itself if it is not a group
        std::vector<const ThemeValue *> itemsOf(const ThemeValue &value) {
            std::vector<const ThemeValue *> items;
            if (value.kind == ThemeValue::Group) {
                for (const auto &argument : value.arguments) {
                    items.push_back(&argument.value);
                }
            } else {
                items.push_back(&value);
            }
            return items;
        }

        void warn(QStringView name, QStringView text, const ThemeError &error) {
            qCWarning(lcTheme).noquote() << QStringLiteral("%1(%2): %3 (at %4)")
                                                .arg(name, text, error.message)
                                                .arg(error.position);
        }

        // Registers the conversions of \a T, written as a function named \a name
        template <class T>
        void registerType(QStringView name) {
            const auto fromArguments = [name](QStringView text) -> std::optional<T> {
                ThemeError error;
                const auto arguments = ThemeSyntax::parseArguments(text, &error);
                auto result = arguments ? T::read(*arguments, &error) : std::nullopt;
                if (!result) {
                    warn(name, text, error);
                }
                return result;
            };
            // A function, which Qt passes as its name and the text inside its parentheses
            QMetaType::registerConverter<QStringList, T>([name,
                                                          fromArguments](const QStringList &list)
                                                             -> std::optional<T> {
                if (list.size() != 2 || list[0] != name) {
                    qCWarning(lcTheme).noquote()
                        << QStringLiteral("%1 was expected, not %2(...)").arg(name, list.value(0));
                    return std::nullopt;
                }
                return fromArguments(list[1]);
            });
            // A word or a string, read as the first argument
            QMetaType::registerConverter<QString, T>(
                [name](const QString &text) -> std::optional<T> {
                    ThemeError error;
                    const auto value = ThemeSyntax::parse(text, &error);
                    if (!value) {
                        warn(name, text, error);
                        return std::nullopt;
                    }
                    if (value->kind == ThemeValue::Function && value->text == name) {
                        auto result = T::read(value->arguments, &error);
                        if (!result) {
                            warn(name, text, error);
                        }
                        return result;
                    }
                    auto result = T::read(
                        {
                            ThemeArgument{QString(), *value}
                    },
                        &error);
                    if (!result) {
                        warn(name, text, error);
                    }
                    return result;
                });
        }

    }

    QPen ThemePen::pen(ThemeButtonState state) const {
        QPen result(color.value(state), width, style, cap, join);
        if (!dashPattern.isEmpty()) {
            // Qt measures the dash pattern in units of the pen width.
            QList<qreal> pattern;
            const double unit = std::max(width, 1.0);
            for (const double dash : dashPattern) {
                pattern.push_back(dash / unit);
            }
            result.setDashPattern(pattern);
            result.setDashOffset(dashOffset / unit);
        }
        result.setMiterLimit(miterLimit);
        result.setCosmetic(cosmetic);
        return result;
    }

    std::optional<ThemePen> ThemePen::read(const std::vector<ThemeArgument> &arguments,
                                           ThemeError *error) {
        const auto bound =
            ThemeArguments::bind(arguments,
                                 {u"color", u"width", u"style", u"cap", u"join", u"dashPattern",
                                  u"dashOffset", u"miterLimit", u"cosmetic"},
                                 error);
        if (!bound) {
            return std::nullopt;
        }
        const auto &b = *bound;
        ThemePen pen;
        if (b[0]) {
            const auto value = colors(*b[0], error);
            if (!value) {
                return std::nullopt;
            }
            pen.color = *value;
        }
        if (b[1]) {
            const auto value = ThemeReader::pixels(*b[1], error);
            if (!value) {
                return std::nullopt;
            }
            pen.width = *value;
        }
        if (b[2]) {
            const auto value = choice<Qt::PenStyle>(*b[2],
                                                    {
                                                        {u"solid",      Qt::SolidLine     },
                                                        {u"dash",       Qt::DashLine      },
                                                        {u"dot",        Qt::DotLine       },
                                                        {u"dashdot",    Qt::DashDotLine   },
                                                        {u"dashdotdot", Qt::DashDotDotLine},
                                                        {u"none",       Qt::NoPen         },
            },
                                                    error);
            if (!value) {
                return std::nullopt;
            }
            pen.style = *value;
        }
        if (b[3]) {
            const auto value = choice<Qt::PenCapStyle>(*b[3],
                                                       {
                                                           {u"flat",   Qt::FlatCap  },
                                                           {u"square", Qt::SquareCap},
                                                           {u"round",  Qt::RoundCap },
            },
                                                       error);
            if (!value) {
                return std::nullopt;
            }
            pen.cap = *value;
        }
        if (b[4]) {
            const auto value = choice<Qt::PenJoinStyle>(*b[4],
                                                        {
                                                            {u"miter", Qt::MiterJoin},
                                                            {u"bevel", Qt::BevelJoin},
                                                            {u"round", Qt::RoundJoin},
            },
                                                        error);
            if (!value) {
                return std::nullopt;
            }
            pen.join = *value;
        }
        if (b[5]) {
            for (const auto item : itemsOf(*b[5])) {
                const auto value = ThemeReader::pixels(*item, error);
                if (!value) {
                    return std::nullopt;
                }
                pen.dashPattern.push_back(*value);
            }
        }
        if (b[6]) {
            const auto value = ThemeReader::pixels(*b[6], error);
            if (!value) {
                return std::nullopt;
            }
            pen.dashOffset = *value;
        }
        if (b[7]) {
            const auto value = ThemeReader::number(*b[7], error);
            if (!value) {
                return std::nullopt;
            }
            pen.miterLimit = *value;
        }
        if (b[8]) {
            const auto value = ThemeReader::boolean(*b[8], error);
            if (!value) {
                return std::nullopt;
            }
            pen.cosmetic = *value;
        }
        return pen;
    }

    bool ThemePen::operator==(const ThemePen &RHS) const {
        return color == RHS.color && width == RHS.width && style == RHS.style && cap == RHS.cap &&
               join == RHS.join && dashPattern == RHS.dashPattern && dashOffset == RHS.dashOffset &&
               miterLimit == RHS.miterLimit && cosmetic == RHS.cosmetic;
    }

    QFont ThemeFont::font(const QFont &base) const {
        QFont result = base;
        if (pixelSize > 0) {
            result.setPixelSize(pixelSize);
        } else if (pointSize > 0) {
            result.setPointSizeF(pointSize);
        }
        if (weight > 0) {
            result.setWeight(QFont::Weight(weight));
        }
        if (italic) {
            result.setItalic(*italic);
        }
        if (!families.isEmpty()) {
            result.setFamilies(families);
        }
        return result;
    }

    std::optional<ThemeFont> ThemeFont::read(const std::vector<ThemeArgument> &arguments,
                                             ThemeError *error) {
        const auto bound = ThemeArguments::bind(
            arguments, {u"color", u"size", u"weight", u"italic", u"family"}, error);
        if (!bound) {
            return std::nullopt;
        }
        const auto &b = *bound;
        ThemeFont font;
        if (b[0]) {
            const auto value = colors(*b[0], error);
            if (!value) {
                return std::nullopt;
            }
            font.color = *value;
        }
        if (b[1]) {
            const auto &size = *b[1];
            bool ok = false;
            if (size.kind == ThemeValue::Word && size.text.endsWith(u"pt")) {
                font.pointSize = QStringView(size.text).chopped(2).toDouble(&ok);
            } else if (const auto pixels = ThemeReader::pixels(size, nullptr)) {
                font.pixelSize = *pixels;
                ok = true;
            }
            if (!ok) {
                return fail<ThemeFont>(error, size.position,
                                       translated(QT_TRANSLATE_NOOP(
                                           "hello::daw::ThemeTypes",
                                           "A size in pixels (px) or points (pt) was expected.")));
            }
        }
        if (b[2]) {
            const auto &weight = *b[2];
            if (const auto number = ThemeReader::integer(weight, nullptr)) {
                font.weight = std::clamp(*number, 1, 1000);
            } else {
                const auto value = choice<int>(weight,
                                               {
                                                   {u"thin",       100},
                                                   {u"extralight", 200},
                                                   {u"light",      300},
                                                   {u"normal",     400},
                                                   {u"medium",     500},
                                                   {u"demibold",   600},
                                                   {u"bold",       700},
                                                   {u"extrabold",  800},
                                                   {u"black",      900},
                },
                                               error);
                if (!value) {
                    return std::nullopt;
                }
                font.weight = *value;
            }
        }
        if (b[3]) {
            const auto value = ThemeReader::boolean(*b[3], error);
            if (!value) {
                return std::nullopt;
            }
            font.italic = *value;
        }
        if (b[4]) {
            for (const auto item : itemsOf(*b[4])) {
                const auto value = ThemeReader::text(*item, error);
                if (!value) {
                    return std::nullopt;
                }
                font.families.push_back(*value);
            }
        }
        return font;
    }

    bool ThemeFont::operator==(const ThemeFont &RHS) const {
        return color == RHS.color && pixelSize == RHS.pixelSize && pointSize == RHS.pointSize &&
               weight == RHS.weight && italic == RHS.italic && families == RHS.families;
    }

    std::optional<ThemeRect> ThemeRect::read(const std::vector<ThemeArgument> &arguments,
                                             ThemeError *error) {
        const auto bound =
            ThemeArguments::bind(arguments, {u"color", u"margins", u"radius"}, error);
        if (!bound) {
            return std::nullopt;
        }
        const auto &b = *bound;
        ThemeRect rect;
        if (b[0]) {
            const auto value = colors(*b[0], error);
            if (!value) {
                return std::nullopt;
            }
            rect.color = *value;
        }
        if (b[1]) {
            std::vector<int> lengths;
            for (const auto item : itemsOf(*b[1])) {
                const auto value = ThemeReader::pixels(*item, error);
                if (!value) {
                    return std::nullopt;
                }
                lengths.push_back(*value);
            }
            switch (lengths.size()) {
                case 1:
                    rect.margins = QMargins(lengths[0], lengths[0], lengths[0], lengths[0]);
                    break;
                case 2:
                    rect.margins = QMargins(lengths[1], lengths[0], lengths[1], lengths[0]);
                    break;
                case 4:
                    rect.margins = QMargins(lengths[0], lengths[1], lengths[2], lengths[3]);
                    break;
                default:
                    return fail<ThemeRect>(error, b[1]->position,
                                           translated(QT_TRANSLATE_NOOP(
                                               "hello::daw::ThemeTypes",
                                               "The margins must be one, two or four lengths.")));
            }
        }
        if (b[2]) {
            const auto value = ThemeReader::pixels(*b[2], error);
            if (!value) {
                return std::nullopt;
            }
            rect.radius = *value;
        }
        return rect;
    }

    bool ThemeRect::operator==(const ThemeRect &RHS) const {
        return color == RHS.color && margins == RHS.margins && radius == RHS.radius;
    }

    std::optional<ThemeShadow> ThemeShadow::read(const std::vector<ThemeArgument> &arguments,
                                                 ThemeError *error) {
        const auto bound = ThemeArguments::bind(arguments, {u"color", u"blur", u"offset"}, error);
        if (!bound) {
            return std::nullopt;
        }
        const auto &b = *bound;
        ThemeShadow shadow;
        if (b[0]) {
            const auto value = ThemeReader::color(*b[0], error);
            if (!value) {
                return std::nullopt;
            }
            shadow.color = *value;
        }
        if (b[1]) {
            const auto value = ThemeReader::pixels(*b[1], error);
            if (!value) {
                return std::nullopt;
            }
            shadow.blur = *value;
        }
        if (b[2]) {
            const auto value = ThemeReader::size(*b[2], error);
            if (!value) {
                return std::nullopt;
            }
            shadow.offset = QPointF(value->width(), value->height());
        }
        return shadow;
    }

    bool ThemeShadow::operator==(const ThemeShadow &RHS) const {
        return color == RHS.color && blur == RHS.blur && offset == RHS.offset;
    }

    void ThemeTypes::registerConversions() {
        static std::once_flag once;
        std::call_once(once, [] {
            registerType<ThemePen>(u"qpen");
            registerType<ThemeFont>(u"qfont");
            registerType<ThemeRect>(u"qrect");
            registerType<ThemeShadow>(u"qshadow");
        });
    }

}
