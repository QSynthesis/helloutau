#include "ThemeReader.h"

#include "ThemeStates.h"

namespace hello::daw {

    QStringView themeStateKey(ThemeButtonState state) {
        static constexpr QStringView keys[] = {u"up",  u"over",  u"down",  u"disabled",
                                               u"up2", u"over2", u"down2", u"disabled2"};
        return keys[size_t(state)];
    }

    namespace {

        template <class T>
        std::optional<T> fail(ThemeError *error, const ThemeValue &value, const QString &message) {
            if (error) {
                *error = {value.position, message};
            }
            return std::nullopt;
        }

        // Reads a color component: a number, or a percentage of \a full
        std::optional<double> component(const ThemeValue &value, double full) {
            if (value.kind != ThemeValue::Word) {
                return std::nullopt;
            }
            bool ok = false;
            if (value.text.endsWith(u'%')) {
                const double percent = QStringView(value.text).chopped(1).toDouble(&ok);
                return ok ? std::optional<double>(percent / 100 * full) : std::nullopt;
            }
            const double number = value.text.toDouble(&ok);
            return ok ? std::optional<double>(number) : std::nullopt;
        }

        // Reads an alpha value: 0 to 255, a fraction written with a decimal point, or a percentage
        std::optional<double> alpha(const ThemeValue &value) {
            const auto number = component(value, 255);
            if (number && value.kind == ThemeValue::Word && !value.text.endsWith(u'%') &&
                value.text.contains(u'.') && *number <= 1) {
                return *number * 255;
            }
            return number;
        }

    }

    std::optional<QColor> ThemeReader::color(const ThemeValue &value, ThemeError *error) {
        if (value.kind == ThemeValue::Word) {
            if (value.text.startsWith(u'#') || QColor::isValidColorName(value.text)) {
                const auto color = QColor::fromString(value.text);
                if (color.isValid()) {
                    return color;
                }
            }
            return fail<QColor>(error, value, tr("\"%1\" is not a color.").arg(value.text));
        }
        if (value.kind != ThemeValue::Function) {
            return fail<QColor>(error, value, tr("A color was expected."));
        }

        const auto &name = value.text;
        const bool withAlpha = name.endsWith(u'a');
        const auto model = withAlpha ? name.chopped(1) : name;
        const size_t count = withAlpha ? 4 : 3;
        if ((model != u"rgb" && model != u"hsv" && model != u"hsl") ||
            value.arguments.size() != count) {
            return fail<QColor>(error, value,
                                tr("A color function must be rgb, hsv or hsl with three "
                                   "components, or rgba, hsva or hsla with four components."));
        }
        double parts[4] = {0, 0, 0, 255};
        for (size_t i = 0; i < count; ++i) {
            const auto &argument = value.arguments[i];
            // The hue is in degrees. The other components range up to 255.
            const auto part =
                i == 3 ? alpha(argument.value)
                       : component(argument.value, model != u"rgb" && i == 0 ? 359 : 255);
            if (!argument.key.isEmpty() || !part) {
                return fail<QColor>(error, argument.value,
                                    tr("A color component must be a number or a percentage."));
            }
            parts[i] = *part;
        }
        const auto clamp = [](double v, int top) { return std::clamp(qRound(v), 0, top); };
        const int a = clamp(parts[3], 255);
        if (model == u"rgb") {
            return QColor(clamp(parts[0], 255), clamp(parts[1], 255), clamp(parts[2], 255), a);
        }
        if (model == u"hsv") {
            return QColor::fromHsv(clamp(parts[0], 359), clamp(parts[1], 255), clamp(parts[2], 255),
                                   a);
        }
        return QColor::fromHsl(clamp(parts[0], 359), clamp(parts[1], 255), clamp(parts[2], 255), a);
    }

    std::optional<int> ThemeReader::pixels(const ThemeValue &value, ThemeError *error) {
        if (value.kind == ThemeValue::Word) {
            if (value.text == u"0") {
                return 0;
            }
            if (value.text.endsWith(u"px")) {
                bool ok = false;
                const double number = QStringView(value.text).chopped(2).toDouble(&ok);
                if (ok) {
                    return qRound(number);
                }
            }
        }
        return fail<int>(error, value, tr("A length in pixels, such as 2px, was expected."));
    }

    std::optional<QSize> ThemeReader::size(const ThemeValue &value, ThemeError *error) {
        if (value.kind == ThemeValue::Sequence && value.items.size() == 2) {
            const auto width = pixels(value.items[0], error);
            const auto height = width ? pixels(value.items[1], error) : std::nullopt;
            return height ? std::optional<QSize>(QSize(*width, *height)) : std::nullopt;
        }
        const auto both = pixels(value, error);
        return both ? std::optional<QSize>(QSize(*both, *both)) : std::nullopt;
    }

    std::optional<double> ThemeReader::number(const ThemeValue &value, ThemeError *error) {
        bool ok = false;
        const double result = value.kind == ThemeValue::Word ? value.text.toDouble(&ok) : 0;
        return ok ? std::optional<double>(result)
                  : fail<double>(error, value, tr("A number was expected."));
    }

    std::optional<int> ThemeReader::integer(const ThemeValue &value, ThemeError *error) {
        bool ok = false;
        const int result = value.kind == ThemeValue::Word ? value.text.toInt(&ok) : 0;
        return ok ? std::optional<int>(result)
                  : fail<int>(error, value, tr("An integer was expected."));
    }

    std::optional<bool> ThemeReader::boolean(const ThemeValue &value, ThemeError *error) {
        if (value.kind == ThemeValue::Word && (value.text == u"true" || value.text == u"false")) {
            return value.text == u"true";
        }
        return fail<bool>(error, value, tr("A boolean value (true or false) was expected."));
    }

    std::optional<QString> ThemeReader::text(const ThemeValue &value, ThemeError *error) {
        if (value.kind == ThemeValue::String || value.kind == ThemeValue::Word) {
            return value.text;
        }
        return fail<QString>(error, value, tr("A string was expected."));
    }

}
