#ifndef HELLOUTAU_THEME_THEMESTATES_H
#define HELLOUTAU_THEME_THEMESTATES_H

#include <array>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QStringView>

#include <helloutau/Theme/ThemeSyntax.h>

namespace hello::daw {

    /// The states in which an interactive element is drawn: up, under the pointer, pressed and
    /// disabled, each unchecked and checked.
    enum class ThemeButtonState {
        Up,
        Over,
        Down,
        Disabled,
        CheckedUp,
        CheckedOver,
        CheckedDown,
        CheckedDisabled,
    };

    /// The keywords of the states in a state group, in the order of ThemeButtonState, which is
    /// also the order of positional values.
    HELLOUTAU_THEME_EXPORT QStringView themeStateKey(ThemeButtonState state);

    /// The state whose value \a state takes when a group does not give it: over and disabled
    /// fall back to up, down to over, and the checked states likewise, their up to up.
    constexpr ThemeButtonState themeStateFallback(ThemeButtonState state) {
        using S = ThemeButtonState;
        constexpr S fallback[8] = {S::Up, S::Up,        S::Over,        S::Up,
                                   S::Up, S::CheckedUp, S::CheckedOver, S::CheckedUp};
        return fallback[size_t(state)];
    }

    /// A value for each ThemeButtonState, as a field that supports button states holds it.
    ///
    /// Written as one value for every state, or as a group such as <tt>(white, down=grey)</tt>
    /// whose missing states fall back: over to up, down to over, disabled to up, up2 to up, and
    /// within the checked states as within the unchecked ones. See the section on the value
    /// syntax in docs/Theme.md.
    template <class T>
    class ThemeStates {
    public:
        ThemeStates() = default;

        explicit ThemeStates(const T &value) {
            m_values.fill(value);
        }

        const T &value(ThemeButtonState state) const {
            return m_values[size_t(state)];
        }

        void setValue(ThemeButtonState state, const T &value) {
            m_values[size_t(state)] = value;
        }

        bool operator==(const ThemeStates &RHS) const {
            return m_values == RHS.m_values;
        }

        /// Reads \a value with \a read, a function from a ThemeValue and a ThemeError pointer to
        /// an optional T, for one value or for each state of a group.
        template <class Read>
        static std::optional<ThemeStates> read(const ThemeValue &value, Read read,
                                               ThemeError *error) {
            if (value.kind != ThemeValue::Group) {
                const auto single = read(value, error);
                return single ? std::optional<ThemeStates>(ThemeStates(*single)) : std::nullopt;
            }

            std::array<std::optional<T>, 8> given;
            size_t next = 0;
            for (const auto &argument : value.arguments) {
                size_t index = next++;
                if (!argument.key.isEmpty()) {
                    index = given.size();
                    for (size_t i = 0; i < given.size(); ++i) {
                        if (argument.key == themeStateKey(ThemeButtonState(i))) {
                            index = i;
                        }
                    }
                    if (index == given.size()) {
                        return fail(error, argument.value.position,
                                    QCoreApplication::translate("hello::daw::ThemeStates",
                                                                "\"%1\" is not a button state.")
                                        .arg(argument.key));
                    }
                } else if (index >= given.size()) {
                    return fail(error, argument.value.position,
                                QCoreApplication::translate("hello::daw::ThemeStates",
                                                            "There are only eight button states."));
                }
                given[index] = read(argument.value, error);
                if (!given[index]) {
                    return std::nullopt;
                }
            }
            if (!given[0]) {
                return fail(error, value.position,
                            QCoreApplication::translate("hello::daw::ThemeStates",
                                                        "The state \"up\" is required."));
            }

            // Each state falls back to one that precedes it, so one pass in order suffices.
            ThemeStates states;
            for (size_t i = 0; i < given.size(); ++i) {
                states.m_values[i] =
                    given[i] ? *given[i]
                             : states.m_values[size_t(themeStateFallback(ThemeButtonState(i)))];
            }
            return states;
        }

    private:
        std::array<T, 8> m_values{};

        static std::optional<ThemeStates> fail(ThemeError *error, qsizetype position,
                                               const QString &message) {
            if (error) {
                *error = {position, message};
            }
            return std::nullopt;
        }
    };

}

#endif // HELLOUTAU_THEME_THEMESTATES_H
