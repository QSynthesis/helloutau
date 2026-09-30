#ifndef HELLOUTAU_THEME_THEMESTATES_H
#define HELLOUTAU_THEME_THEMESTATES_H

#include <array>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QStringView>

#include <helloutau/Theme/ThemeSyntax.h>

namespace hello::daw {

    /// The states in which an interactive element is drawn: up, hovered, pressed and disabled,
    /// each unchecked and checked.
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

    /// Returns the keyword of \a state in a state group. The order of ThemeButtonState is also
    /// the order of positional values.
    HELLOUTAU_THEME_EXPORT QStringView themeStateKey(ThemeButtonState state);

    /// Returns the state from which \a state takes its value if a group omits \a state. Over and
    /// disabled fall back to up, and down falls back to over. The checked states fall back in
    /// the same way among themselves, and checked up falls back to up.
    constexpr ThemeButtonState themeStateFallback(ThemeButtonState state) {
        using S = ThemeButtonState;
        constexpr S fallback[8] = {S::Up, S::Up,        S::Over,        S::Up,
                                   S::Up, S::CheckedUp, S::CheckedOver, S::CheckedUp};
        return fallback[size_t(state)];
    }

    /// A value for each ThemeButtonState, as stored by a field that supports button states.
    ///
    /// The syntax is one value for every state, or a group such as <tt>(white, down=grey)</tt>.
    /// The states that a group omits fall back: over to up, down to over, disabled to up, up2 to
    /// up, and among the checked states as among the unchecked states. See the section on the
    /// value syntax in docs/Theme.md.
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

        /// Reads \a value as one value or as a group of states, each by \a read, a function from
        /// a ThemeValue and a ThemeError pointer to an optional T. Returns \c std::nullopt, with
        /// the reason in \a error, if a value is malformed, a key is not a state, the group has
        /// more than eight values, or the group omits \c up.
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
                    return fail(
                        error, argument.value.position,
                        QCoreApplication::translate("hello::daw::ThemeStates",
                                                    "A group has at most eight button states."));
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

            // Because each state falls back to a preceding state, a single pass in order suffices.
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
