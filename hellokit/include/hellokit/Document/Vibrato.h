#ifndef HELLOKIT_DOCUMENT_VIBRATO_H
#define HELLOKIT_DOCUMENT_VIBRATO_H

#include <QtCore/QDataStream>
#include <QtCore/QJsonObject>

#include <hellokit/Document/HelloKitDocumentGlobal.h>

namespace hello::kit {

    /// The vibrato of a note, the values of \c VBR in UST.
    struct HELLOKIT_DOCUMENT_EXPORT Vibrato {
        double length = 0;    ///< percent of the note
        double period = 0;    ///< milliseconds
        double amplitude = 0; ///< cents
        double attack = 0;    ///< percent
        double release = 0;   ///< percent
        double phase = 0;     ///< percent
        double offset = 0;    ///< percent

        /// The eighth value of \c VBR in UST, which UTAU ignores.
        ///
        /// Retained so that a round trip through \c .ust preserves it. The UTAU interface
        /// exposes no field for it, and it cannot be stored in \c Note::userData , because each
        /// value there is written as a separate entry, whereas this one is part of \c VBR.
        double intensity = 0;

        inline bool operator==(const Vibrato &RHS) const {
            return length == RHS.length && period == RHS.period && amplitude == RHS.amplitude &&
                   attack == RHS.attack && release == RHS.release && phase == RHS.phase &&
                   offset == RHS.offset && intensity == RHS.intensity;
        }

        inline bool operator!=(const Vibrato &RHS) const {
            return !(*this == RHS);
        }

        QJsonObject toJson() const;

        /// Returns the vibrato written as in \c .usth. An absent parameter is read as zero.
        static Vibrato fromJson(const QJsonObject &object);

        /// Returns the vibrato that UTAU gives a note, that of \c utau::Vibrato.
        static Vibrato utauDefault();
    };

    // The stream operators of a value stored as a whole in the edit history. They are declared
    // with the type, because Qt records the stream operators of a type where its meta-type is
    // first instantiated. The format is part of the history format and must not change.

    inline QDataStream &operator<<(QDataStream &out, const Vibrato &vibrato) {
        return out << vibrato.length << vibrato.period << vibrato.amplitude << vibrato.attack
                   << vibrato.release << vibrato.phase << vibrato.offset << vibrato.intensity;
    }

    inline QDataStream &operator>>(QDataStream &in, Vibrato &vibrato) {
        return in >> vibrato.length >> vibrato.period >> vibrato.amplitude >> vibrato.attack >>
               vibrato.release >> vibrato.phase >> vibrato.offset >> vibrato.intensity;
    }

}

#endif // HELLOKIT_DOCUMENT_VIBRATO_H
