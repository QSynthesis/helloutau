#include "ThemeArguments_p.h"

#include <algorithm>

namespace hello::daw {

    std::optional<std::vector<const ThemeValue *>>
        ThemeArguments::bind(const std::vector<ThemeArgument> &arguments,
                             std::initializer_list<QStringView> names, ThemeError *error) {
        const auto fail = [error](qsizetype position, const QString &message) {
            if (error) {
                *error = {position, message};
            }
            return std::nullopt;
        };
        std::vector<const ThemeValue *> bound(names.size(), nullptr);
        size_t next = 0;
        for (const auto &argument : arguments) {
            size_t index = next++;
            if (!argument.key.isEmpty()) {
                const auto found = std::find(names.begin(), names.end(), argument.key);
                if (found == names.end()) {
                    return fail(argument.value.position,
                                tr("\"%1\" is not a parameter here.").arg(argument.key));
                }
                index = size_t(found - names.begin());
            } else if (index >= names.size()) {
                return fail(argument.value.position, tr("There are too many values."));
            }
            if (bound[index]) {
                return fail(argument.value.position,
                            tr("\"%1\" is given twice.").arg(*(names.begin() + index)));
            }
            bound[index] = &argument.value;
        }
        return bound;
    }

}
