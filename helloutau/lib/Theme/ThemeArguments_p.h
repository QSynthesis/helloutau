#ifndef HELLOUTAU_THEME_THEMEARGUMENTS_P_H
#define HELLOUTAU_THEME_THEMEARGUMENTS_P_H

#include <initializer_list>
#include <optional>
#include <vector>

#include <QtCore/QCoreApplication>

#include "ThemeSyntax.h"

namespace hello::daw {

    /// The binding of the arguments of a function of the value syntax to its parameters, shared
    /// by the readers of such functions.
    class ThemeArguments {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::ThemeArguments)
    public:
        /// Binds \a arguments to the parameters \a names, by position and then by keyword. Returns
        /// the bound values in the order of \a names, with null for an absent parameter. Returns
        /// \c std::nullopt, with the reason in \a error, for an unknown keyword, a parameter
        /// specified twice or an excess value.
        static std::optional<std::vector<const ThemeValue *>>
            bind(const std::vector<ThemeArgument> &arguments,
                 std::initializer_list<QStringView> names, ThemeError *error);
    };

}

#endif // HELLOUTAU_THEME_THEMEARGUMENTS_P_H
