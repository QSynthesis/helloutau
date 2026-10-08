#include "ModifierBindings.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>

namespace hello::daw {

    namespace {

        constexpr auto standardModifiers =
            Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier;

        // The names of the modifiers in the keymap file
        const std::pair<Qt::KeyboardModifier, const char *> modifierNames[] = {
            {Qt::ControlModifier, "Ctrl" },
            {Qt::AltModifier,     "Alt"  },
            {Qt::ShiftModifier,   "Shift"},
            {Qt::MetaModifier,    "Meta" },
        };

        QJsonArray modifiersToJson(Qt::KeyboardModifiers modifiers) {
            QJsonArray result;
            for (const auto &[modifier, name] : modifierNames) {
                if (modifiers & modifier) {
                    result.push_back(QLatin1String(name));
                }
            }
            return result;
        }

        // The modifiers named in array, and whether every element names one
        std::pair<Qt::KeyboardModifiers, bool> modifiersFromJson(const QJsonArray &array) {
            Qt::KeyboardModifiers result;
            bool known = true;
            for (const auto &part : array) {
                const auto it = std::find_if(
                    std::begin(modifierNames), std::end(modifierNames),
                    [&part](const auto &entry) { return part.toString() == QLatin1String(entry.second); });
                if (it == std::end(modifierNames)) {
                    known = false;
                } else {
                    result |= it->first;
                }
            }
            return {result, known};
        }

    }

    ModifierScheme::ModifierScheme(const char *key, const char *context, const char *name,
                                   QList<Role> roles)
        : m_key(key), m_context(context), m_name(name), m_roles(std::move(roles)) {
        for (int i = 0; i < m_roles.size(); ++i) {
            Q_ASSERT(m_roles.at(i).id == i);
        }
    }

    QString ModifierScheme::key() const {
        return QLatin1String(m_key);
    }

    QString ModifierScheme::name() const {
        return QCoreApplication::translate(m_context, m_name);
    }

    const QList<ModifierScheme::Role> &ModifierScheme::roles() const {
        return m_roles;
    }

    QString ModifierScheme::roleName(int role) const {
        return QCoreApplication::translate(m_context, m_roles.at(role).name);
    }

    ModifierBindings::ModifierBindings(const ModifierScheme &scheme) : m_scheme(&scheme) {
        for (const auto &role : scheme.roles()) {
            m_modifiers.push_back(role.defaults);
        }
    }

    const ModifierScheme &ModifierBindings::scheme() const {
        return *m_scheme;
    }

    Qt::KeyboardModifiers ModifierBindings::modifiers(int role) const {
        return m_modifiers.at(role);
    }

    void ModifierBindings::setModifiers(int role, Qt::KeyboardModifiers modifiers) {
        m_modifiers[role] = modifiers & standardModifiers;
    }

    bool ModifierBindings::matches(int role, Qt::KeyboardModifiers actual) const {
        const auto pressed = actual & standardModifiers;
        const auto bound = m_modifiers.at(role);
        switch (m_scheme->roles().at(role).match) {
            case ModifierScheme::Exact:
                return pressed == bound;
            case ModifierScheme::Contains:
                return bound != Qt::NoModifier && (pressed & bound) == bound;
        }
        return false;
    }

    QList<ModifierBindings::Conflict> ModifierBindings::conflicts() const {
        QList<Conflict> result;
        const auto &roles = m_scheme->roles();
        for (int i = 0; i < roles.size(); ++i) {
            for (int j = i + 1; j < roles.size(); ++j) {
                if (!(roles.at(i).conflictSets & roles.at(j).conflictSets) ||
                    roles.at(i).match != roles.at(j).match) {
                    continue;
                }
                const auto first = m_modifiers.at(i);
                const auto second = m_modifiers.at(j);
                const bool conflict = roles.at(i).match == ModifierScheme::Exact
                                          ? first == second
                                          : (first & second) != Qt::NoModifier;
                if (conflict) {
                    result.push_back({i, j});
                }
            }
        }
        return result;
    }

    QList<int> ModifierBindings::unboundRoles() const {
        QList<int> result;
        const auto &roles = m_scheme->roles();
        for (int i = 0; i < roles.size(); ++i) {
            if (roles.at(i).match == ModifierScheme::Exact && m_modifiers.at(i) == Qt::NoModifier) {
                result.push_back(i);
            }
        }
        return result;
    }

    bool ModifierBindings::isValid() const {
        return conflicts().isEmpty() && unboundRoles().isEmpty();
    }

    QJsonObject ModifierBindings::toJson() const {
        QJsonObject result;
        const auto &roles = m_scheme->roles();
        for (int i = 0; i < roles.size(); ++i) {
            if (m_modifiers.at(i) != roles.at(i).defaults) {
                result.insert(QLatin1String(roles.at(i).key), modifiersToJson(m_modifiers.at(i)));
            }
        }
        return result;
    }

    bool ModifierBindings::readJson(const QJsonObject &object) {
        const auto &roles = m_scheme->roles();
        bool complete = true;
        int known = 0;
        for (int i = 0; i < roles.size(); ++i) {
            const auto value = object.value(QLatin1String(roles.at(i).key));
            if (value.isUndefined()) {
                continue;
            }
            ++known;
            if (!value.isArray()) {
                complete = false;
                continue;
            }
            const auto [modifiers, named] = modifiersFromJson(value.toArray());
            m_modifiers[i] = modifiers;
            complete = complete && named;
        }
        return complete && known == object.size();
    }

    bool ModifierBindings::operator==(const ModifierBindings &other) const {
        return m_scheme == other.m_scheme && m_modifiers == other.m_modifiers;
    }

    bool ModifierBindings::operator!=(const ModifierBindings &other) const {
        return !(*this == other);
    }

}
