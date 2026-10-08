#include "ModifierBindings.h"

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

        Qt::KeyboardModifiers modifiersFromJson(const QJsonArray &array) {
            Qt::KeyboardModifiers result;
            for (const auto &part : array) {
                for (const auto &[modifier, name] : modifierNames) {
                    if (part.toString() == QLatin1String(name)) {
                        result |= modifier;
                    }
                }
            }
            return result;
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
        if (bound == Qt::NoModifier) {
            return pressed == Qt::NoModifier;
        }
        switch (m_scheme->roles().at(role).match) {
            case ModifierScheme::Exact:
                return pressed == bound;
            case ModifierScheme::Contains:
                return (pressed & bound) == bound;
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
                                          ? first != Qt::NoModifier && first == second
                                          : (first & second) != Qt::NoModifier;
                if (conflict) {
                    result.push_back({i, j});
                }
            }
        }
        return result;
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

    void ModifierBindings::readJson(const QJsonObject &object) {
        const auto &roles = m_scheme->roles();
        for (int i = 0; i < roles.size(); ++i) {
            const auto value = object.value(QLatin1String(roles.at(i).key));
            if (value.isArray()) {
                m_modifiers[i] = modifiersFromJson(value.toArray());
            }
        }
    }

    bool ModifierBindings::operator==(const ModifierBindings &other) const {
        return m_scheme == other.m_scheme && m_modifiers == other.m_modifiers;
    }

    bool ModifierBindings::operator!=(const ModifierBindings &other) const {
        return !(*this == other);
    }

}
