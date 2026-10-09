#include "ModifierBindings.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>
#include <QtCore/QMap>

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

        QJsonValue modifiersToJson(std::optional<Qt::KeyboardModifiers> modifiers) {
            if (!modifiers) {
                return QJsonValue::Null;
            }
            QJsonArray result;
            for (const auto &[modifier, name] : modifierNames) {
                if (*modifiers & modifier) {
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
                const auto it = std::find_if(std::begin(modifierNames), std::end(modifierNames),
                                             [&part](const auto &entry) {
                                                 return part.toString() ==
                                                        QLatin1String(entry.second);
                                             });
                if (it == std::end(modifierNames)) {
                    known = false;
                } else {
                    result |= it->first;
                }
            }
            return {result, known};
        }

        bool overlap(Qt::KeyboardModifiers first, Qt::KeyboardModifiers second) {
            return (first & second) != Qt::NoModifier;
        }

    }

    ModifierScheme::ModifierScheme(const char *key, const char *context, const char *name,
                                   QList<Role> roles, QList<Scene> scenes)
        : m_key(key), m_context(context), m_name(name), m_roles(std::move(roles)),
          m_scenes(std::move(scenes)) {
        for (int i = 0; i < m_roles.size(); ++i) {
            Q_ASSERT(m_roles.at(i).id == i);
        }
        for (int i = 0; i < m_scenes.size(); ++i) {
            Q_ASSERT(m_scenes.at(i).id == i);
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

    const QList<ModifierScheme::Scene> &ModifierScheme::scenes() const {
        return m_scenes;
    }

    QString ModifierScheme::roleName(int role) const {
        return QCoreApplication::translate(m_context, m_roles.at(role).name);
    }

    bool ModifierScheme::isStart(int role) const {
        return std::any_of(m_scenes.begin(), m_scenes.end(), [role](const Scene &scene) {
            return std::any_of(scene.operations.begin(), scene.operations.end(),
                               [role](const Operation &operation) {
                                   return operation.role == role;
                               });
        });
    }

    bool ModifierBindings::Activation::isOn(int toggle) const {
        return toggles.contains(toggle);
    }

    bool ModifierBindings::Activation::operator==(const Activation &other) const {
        return scene == other.scene && operation == other.operation && toggles == other.toggles;
    }

    bool ModifierBindings::Conflict::operator==(const Conflict &other) const {
        return scene == other.scene && first == other.first && second == other.second;
    }

    ModifierBindings::ModifierBindings(const ModifierScheme &scheme) : m_scheme(&scheme) {
        for (const auto &role : scheme.roles()) {
            m_modifiers.push_back(role.defaults);
        }
    }

    const ModifierScheme &ModifierBindings::scheme() const {
        return *m_scheme;
    }

    std::optional<Qt::KeyboardModifiers> ModifierBindings::modifiers(int role) const {
        return m_modifiers.at(role);
    }

    void ModifierBindings::setModifiers(int role,
                                        std::optional<Qt::KeyboardModifiers> modifiers) {
        if (modifiers) {
            *modifiers &= standardModifiers;
        }
        m_modifiers[role] = modifiers;
    }

    std::optional<Qt::KeyboardModifiers> ModifierBindings::toggleModifiers(int role) const {
        const auto bound = m_modifiers.at(role);
        if (!bound || *bound == Qt::NoModifier) {
            return std::nullopt;
        }
        return bound;
    }

    std::optional<QList<int>>
        ModifierBindings::togglesFor(const ModifierScheme::Operation &operation,
                                     Qt::KeyboardModifiers rest) const {
        QList<int> result;
        Qt::KeyboardModifiers sum;
        for (const int toggle : operation.toggles) {
            const auto bound = toggleModifiers(toggle);
            if (bound && (rest & *bound) == *bound) {
                result.push_back(toggle);
                sum |= *bound;
            }
        }
        if (sum != rest) {
            return std::nullopt;
        }
        return result;
    }

    std::optional<ModifierBindings::Activation>
        ModifierBindings::activate(int scene, Qt::KeyboardModifiers actual) const {
        const auto pressed = actual & standardModifiers;
        const auto &operations = m_scheme->scenes().at(scene).operations;
        for (const auto &operation : operations) {
            if (m_modifiers.at(operation.role) == pressed) {
                return Activation{scene, operation.role, {}};
            }
        }
        std::optional<Activation> result;
        for (const auto &operation : operations) {
            const auto start = m_modifiers.at(operation.role);
            if (!start || (pressed & *start) != *start) {
                continue;
            }
            if (const auto toggles = togglesFor(operation, pressed & ~*start)) {
                if (result) {
                    return std::nullopt;
                }
                result = Activation{scene, operation.role, *toggles};
            }
        }
        return result;
    }

    void ModifierBindings::updateToggles(Activation &activation,
                                         Qt::KeyboardModifiers actual) const {
        const auto &operations = m_scheme->scenes().at(activation.scene).operations;
        const auto it = std::find_if(operations.begin(), operations.end(),
                                     [&activation](const ModifierScheme::Operation &operation) {
                                         return operation.role == activation.operation;
                                     });
        if (it == operations.end()) {
            return;
        }
        const auto start = m_modifiers.at(it->role).value_or(Qt::KeyboardModifiers());
        if (const auto toggles = togglesFor(*it, actual & standardModifiers & ~start)) {
            activation.toggles = *toggles;
        }
    }

    bool ModifierBindings::isHeld(int role, Qt::KeyboardModifiers actual) const {
        const auto bound = toggleModifiers(role);
        return bound && (actual & standardModifiers) == *bound;
    }

    QList<ModifierBindings::Conflict> ModifierBindings::conflicts() const {
        QList<Conflict> result;
        const auto add = [&result](const Conflict &conflict) {
            if (!result.contains(conflict)) {
                result.push_back(conflict);
            }
        };
        for (const auto &scene : m_scheme->scenes()) {
            const auto &operations = scene.operations;
            for (int i = 0; i < operations.size(); ++i) {
                const auto &operation = operations.at(i);
                const auto start = m_modifiers.at(operation.role);
                // Operations that start with the same modifiers
                for (int j = i + 1; j < operations.size(); ++j) {
                    if (start && start == m_modifiers.at(operations.at(j).role)) {
                        add({scene.id, operation.role, operations.at(j).role});
                    }
                }
                const auto &toggles = operation.toggles;
                for (int k = 0; k < toggles.size(); ++k) {
                    const auto bound = m_modifiers.at(toggles.at(k));
                    if (!bound) {
                        continue;
                    }
                    if (*bound == Qt::NoModifier) {
                        add({scene.id, toggles.at(k), std::nullopt});
                        continue;
                    }
                    // A toggle that shares modifiers with the start of its operation
                    if (start && overlap(*start, *bound)) {
                        add({scene.id, operation.role, toggles.at(k)});
                    }
                    // Toggles of one operation that share modifiers
                    for (int l = k + 1; l < toggles.size(); ++l) {
                        const auto other = toggleModifiers(toggles.at(l));
                        if (other && overlap(*bound, *other)) {
                            add({scene.id, toggles.at(k), toggles.at(l)});
                        }
                    }
                }
            }
        }
        return result;
    }

    QList<ModifierBindings::Ambiguity> ModifierBindings::ambiguities() const {
        QList<Ambiguity> result;
        for (const auto &scene : m_scheme->scenes()) {
            // Every way to hold modifiers that selects an operation, by the modifiers
            QMap<int, QList<Activation>> ways;
            QList<Qt::KeyboardModifiers> starts;
            for (const auto &operation : scene.operations) {
                const auto start = m_modifiers.at(operation.role);
                if (!start) {
                    continue;
                }
                starts.push_back(*start);
                QList<int> toggles;
                for (const int toggle : operation.toggles) {
                    const auto bound = toggleModifiers(toggle);
                    if (bound && !overlap(*start, *bound)) {
                        toggles.push_back(toggle);
                    }
                }
                for (int subset = 0; subset < (1 << toggles.size()); ++subset) {
                    auto held = *start;
                    QList<int> on;
                    bool disjoint = true;
                    for (int k = 0; k < toggles.size(); ++k) {
                        if (subset & (1 << k)) {
                            const auto bound = *toggleModifiers(toggles.at(k));
                            disjoint = disjoint && !overlap(held, bound);
                            held |= bound;
                            on.push_back(toggles.at(k));
                        }
                    }
                    if (disjoint) {
                        ways[held.toInt()].push_back({scene.id, operation.role, on});
                    }
                }
            }
            for (auto it = ways.begin(); it != ways.end(); ++it) {
                const auto held = Qt::KeyboardModifiers::fromInt(it.key());
                if (it.value().size() > 1 && !starts.contains(held)) {
                    result.push_back({scene.id, held, it.value()});
                }
            }
        }
        return result;
    }

    bool ModifierBindings::isValid() const {
        return conflicts().isEmpty();
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
            if (value.isNull()) {
                m_modifiers[i] = std::nullopt;
            } else if (value.isArray()) {
                const auto [modifiers, named] = modifiersFromJson(value.toArray());
                m_modifiers[i] = modifiers;
                complete = complete && named;
            } else {
                complete = false;
            }
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
