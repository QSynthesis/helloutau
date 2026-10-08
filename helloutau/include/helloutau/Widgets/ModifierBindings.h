#ifndef HELLOUTAU_WIDGETS_MODIFIERBINDINGS_H
#define HELLOUTAU_WIDGETS_MODIFIERBINDINGS_H

#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/Qt>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// The roles of the modifier keys in the interactions of one view, such as the note area of
    /// the piano roll. The roles of one scheme are independent of the roles of every other
    /// scheme. Two schemes may bind the same modifiers, and conflicts exist within a scheme only.
    ///
    /// A class of its own describes each scheme: it declares the roles as a plain enumeration
    /// numbered from 0 and returns the scheme, see NoteViewModifiers. A view that uses roles of
    /// another scheme instead of declaring them states so under that enumeration.
    class HELLOUTAU_WIDGETS_EXPORT ModifierScheme {
    public:
        /// The way in which the modifiers of an event match the modifiers of a role. Only Ctrl,
        /// Alt, Shift and Meta count.
        enum Match {
            /// The modifiers of the event are exactly those of the role, for a role that selects
            /// the action of a press or a wheel step. The role requires modifiers, because the
            /// event without modifiers has an action of its own.
            Exact,

            /// The modifiers of the event include those of the role, for a role that changes a
            /// drag while its modifiers are held. The role without modifiers matches no event,
            /// which turns its action off.
            Contains,
        };

        struct Role {
            /// The value of the role in the enumeration of the scheme, equal to its index in
            /// roles()
            int id = 0;

            /// The key of the role in the keymap file
            const char *key = nullptr;

            /// The name of the role, translated in the context of the scheme
            const char *name = nullptr;

            Match match = Exact;

            /// The conflict sets of the role, one bit each. Two roles conflict only if they share
            /// a set: two roles with Exact must not have the same modifiers, and two roles with
            /// Contains must not share a modifier. A role with Exact and a role with Contains do
            /// not conflict.
            quint32 conflictSets = 0;

            Qt::KeyboardModifiers defaults;
        };

        /// Creates the scheme \a key with \a roles in the order of their ids. \a context is the
        /// translation context of \a name and of the names of the roles.
        ModifierScheme(const char *key, const char *context, const char *name, QList<Role> roles);

        /// Returns the key of the scheme in the section of its kind of window in the keymap
        /// file.
        QString key() const;

        /// Returns the translated name of the scheme.
        QString name() const;

        const QList<Role> &roles() const;

        /// Returns the translated name of \a role.
        QString roleName(int role) const;

    private:
        const char *m_key;
        const char *m_context;
        const char *m_name;
        QList<Role> m_roles;
    };

    /// The modifiers bound to each role of a ModifierScheme. The scheme must outlive the
    /// bindings.
    class HELLOUTAU_WIDGETS_EXPORT ModifierBindings {
    public:
        /// Two roles that share a conflict set and whose modifiers conflict
        struct Conflict {
            int first = 0;
            int second = 0;
        };

        /// Creates the bindings of \a scheme with the defaults of its roles.
        explicit ModifierBindings(const ModifierScheme &scheme);

        const ModifierScheme &scheme() const;

        Qt::KeyboardModifiers modifiers(int role) const;
        void setModifiers(int role, Qt::KeyboardModifiers modifiers);

        /// Returns whether \a actual matches the modifiers of \a role in the way of its
        /// ModifierScheme::Match.
        bool matches(int role, Qt::KeyboardModifiers actual) const;

        /// Returns the pairs of roles that conflict, the lower id first.
        QList<Conflict> conflicts() const;

        /// Returns the roles with ModifierScheme::Exact that have no modifiers.
        QList<int> unboundRoles() const;

        /// Returns whether no roles conflict and no roles are unbound.
        bool isValid() const;

        /// Returns the modifiers of the roles that differ from their defaults, keyed by the keys
        /// of the roles, each an array of \c Ctrl, \c Alt, \c Shift and \c Meta.
        QJsonObject toJson() const;

        /// Sets the modifiers of the roles in \a object, which has the form of toJson(), and
        /// ignores unknown keys, values that are not arrays and unknown modifier names.
        /// \return \c true if \a object holds nothing that is ignored, \c false otherwise.
        bool readJson(const QJsonObject &object);

        bool operator==(const ModifierBindings &other) const;
        bool operator!=(const ModifierBindings &other) const;

    private:
        const ModifierScheme *m_scheme;
        QList<Qt::KeyboardModifiers> m_modifiers;
    };

}

#endif // HELLOUTAU_WIDGETS_MODIFIERBINDINGS_H
