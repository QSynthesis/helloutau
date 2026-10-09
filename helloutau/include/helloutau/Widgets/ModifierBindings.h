#ifndef HELLOUTAU_WIDGETS_MODIFIERBINDINGS_H
#define HELLOUTAU_WIDGETS_MODIFIERBINDINGS_H

#include <optional>

#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/Qt>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// The roles of the modifier keys in the interactions of one view, such as the note area of
    /// the piano roll, and the scenes in which they act. The roles of one scheme are independent
    /// of the roles of every other scheme. Two schemes may bind the same modifiers. See the
    /// modifier keys in docs/Widgets.md.
    ///
    /// A scene is one table of operations, such as the drags that start on the end of a note.
    /// The modifiers held when a drag starts, or when a click is released, select one operation
    /// of the scene, which then runs until the button is released. While the operation runs,
    /// its toggles turn on and off with the modifiers held.
    ///
    /// A class of its own describes each scheme: it declares the roles and the scenes as plain
    /// enumerations numbered from 0 and returns the scheme, see NoteViewModifiers. A view that
    /// uses roles of another scheme instead of declaring them states so under that enumeration.
    ///
    /// Only Ctrl, Alt, Shift and Meta count as modifiers. A role may be off, which differs from
    /// a role without modifiers.
    class HELLOUTAU_WIDGETS_EXPORT ModifierScheme {
    public:
        struct Role {
            /// The value of the role in the enumeration of the scheme, equal to its index in
            /// roles()
            int id = 0;

            /// The key of the role in the keymap file
            const char *key = nullptr;

            /// The name of the role, translated in the context of the scheme
            const char *name = nullptr;

            Qt::KeyboardModifiers defaults;
        };

        /// An operation of a scene: the role whose modifiers start it and the roles that toggle
        /// while it runs
        struct Operation {
            int role = 0;
            QList<int> toggles;
        };

        struct Scene {
            /// The value of the scene in the enumeration of the scheme, equal to its index in
            /// scenes()
            int id = 0;

            QList<Operation> operations;
        };

        /// Creates the scheme \a key with \a roles and \a scenes in the order of their ids.
        /// \a context is the translation context of \a name and of the names of the roles.
        ModifierScheme(const char *key, const char *context, const char *name, QList<Role> roles,
                       QList<Scene> scenes);

        /// Returns the key of the scheme in the section of its kind of window in the keymap
        /// file.
        QString key() const;

        /// Returns the translated name of the scheme.
        QString name() const;

        const QList<Role> &roles() const;
        const QList<Scene> &scenes() const;

        /// Returns the translated name of \a role.
        QString roleName(int role) const;

        /// Returns whether \a role starts an operation in a scene and may therefore have no
        /// modifiers. A role that only toggles requires modifiers.
        bool isStart(int role) const;

    private:
        const char *m_key;
        const char *m_context;
        const char *m_name;
        QList<Role> m_roles;
        QList<Scene> m_scenes;
    };

    /// The modifiers bound to each role of a ModifierScheme. The scheme must outlive the
    /// bindings.
    class HELLOUTAU_WIDGETS_EXPORT ModifierBindings {
    public:
        /// An operation of a scene that modifiers select, with the toggles that are on
        struct HELLOUTAU_WIDGETS_EXPORT Activation {
            int scene = 0;

            /// The role that starts the operation
            int operation = 0;

            QList<int> toggles;

            /// Returns whether \a toggle is on.
            bool isOn(int toggle) const;

            bool operator==(const Activation &other) const;
        };

        /// Two roles of a scene that cannot both act, or one role that can never act. A
        /// conflict leaves the bindings invalid.
        struct HELLOUTAU_WIDGETS_EXPORT Conflict {
            int scene = 0;
            int first = 0;

            /// The other role, or \c std::nullopt if \a first is a toggle without modifiers
            std::optional<int> second;

            bool operator==(const Conflict &other) const;
        };

        /// Modifiers that split in more than one way into the modifiers that start an operation
        /// of a scene and the modifiers of its toggles, while no operation starts with exactly
        /// these modifiers. activate() selects nothing for them. An ambiguity leaves the bindings
        /// valid.
        struct Ambiguity {
            int scene = 0;
            Qt::KeyboardModifiers modifiers;
            QList<Activation> activations;
        };

        /// Creates the bindings of \a scheme with the defaults of its roles.
        explicit ModifierBindings(const ModifierScheme &scheme);

        const ModifierScheme &scheme() const;

        /// Returns the modifiers of \a role, or \c std::nullopt if the role is off.
        std::optional<Qt::KeyboardModifiers> modifiers(int role) const;

        /// Sets the modifiers of \a role, or turns the role off with \c std::nullopt.
        void setModifiers(int role, std::optional<Qt::KeyboardModifiers> modifiers);

        /// Returns the operation of \a scene that \a actual selects: the operation that starts
        /// with exactly \a actual, otherwise the only way to split \a actual into the modifiers
        /// that start an operation and those of some of its toggles, which are then on. Roles
        /// that are off take no part.
        /// \return \c std::nullopt if no operation starts with \a actual and \a actual splits in
        /// no way or in more than one way.
        std::optional<Activation> activate(int scene, Qt::KeyboardModifiers actual) const;

        /// Sets the toggles of \a activation from \a actual, without the modifiers that start
        /// its operation: the toggles whose modifiers add up to exactly the rest turn on, the
        /// others off. Leaves the toggles as they are if no toggles add up to the rest.
        void updateToggles(Activation &activation, Qt::KeyboardModifiers actual) const;

        /// Returns whether \a actual is exactly the modifiers of \a role, which is on and has
        /// modifiers. For a view that uses a toggle of another view outside any operation.
        bool isHeld(int role, Qt::KeyboardModifiers actual) const;

        QList<Conflict> conflicts() const;
        QList<Ambiguity> ambiguities() const;

        /// Returns whether no roles conflict.
        bool isValid() const;

        /// Returns the modifiers of the roles that differ from their defaults, keyed by the keys
        /// of the roles, each an array of \c Ctrl, \c Alt, \c Shift and \c Meta, or \c null for
        /// a role that is off.
        QJsonObject toJson() const;

        /// Sets the modifiers of the roles in \a object, which has the form of toJson(), and
        /// ignores unknown keys, values that are neither arrays nor \c null and unknown modifier
        /// names.
        /// \return \c true if \a object holds nothing that is ignored, \c false otherwise.
        bool readJson(const QJsonObject &object);

        bool operator==(const ModifierBindings &other) const;
        bool operator!=(const ModifierBindings &other) const;

    private:
        const ModifierScheme *m_scheme;
        QList<std::optional<Qt::KeyboardModifiers>> m_modifiers;

        // The modifiers of role if it is on and has modifiers
        std::optional<Qt::KeyboardModifiers> toggleModifiers(int role) const;

        // The toggles of operation whose modifiers add up to exactly rest
        std::optional<QList<int>> togglesFor(const ModifierScheme::Operation &operation,
                                             Qt::KeyboardModifiers rest) const;
    };

}

#endif // HELLOUTAU_WIDGETS_MODIFIERBINDINGS_H
