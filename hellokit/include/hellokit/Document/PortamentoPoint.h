#ifndef HELLOKIT_DOCUMENT_PORTAMENTOPOINT_H
#define HELLOKIT_DOCUMENT_PORTAMENTOPOINT_H

#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringView>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Document/HelloKitDocumentGlobal.h>

namespace hello::kit {

    /// One control point of the Mode2 pitch curve.
    struct HELLOKIT_DOCUMENT_EXPORT PortamentoPoint {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::PortamentoPoint)
    public:
        /// The curve shape connecting a point to the preceding point.
        ///
        /// \warning The letters in the \c PBM entry of UST do not match these names. An empty
        ///          letter denotes \c S, \c s denotes \c Linear, \c r denotes \c R and \c j
        ///          denotes \c J. The mapping is stated here deliberately, so that only the UST
        ///          reader and writer depend on it.
        enum Type {
            S,
            Linear,
            R,
            J,
        };

        /// In milliseconds from the start of the note, for every point. Negative if the curve
        /// extends into the preceding note.
        ///
        /// \note UST writes the first point in \c PBS relative to the start of the note, and each
        ///       subsequent point in \c PBW as the interval from the preceding point.
        double x = 0;

        /// In cents.
        ///
        /// \note UST writes the height in tenths of a semitone, see centsFromTenths().
        double y = 0;

        Type type = S;

        inline bool operator==(const PortamentoPoint &RHS) const {
            return x == RHS.x && y == RHS.y && type == RHS.type;
        }

        /// Converts a height in tenths of a semitone, as \c PBS and \c PBY write it, to cents.
        /// The result is rounded to a millionth of a cent, so that a decimal read from a file
        /// gives the same decimal in cents rather than a value that differs in the last binary
        /// digit.
        static double centsFromTenths(double tenths);

        /// Converts a height in cents to tenths of a semitone, as \c PBS and \c PBY write it.
        static double tenthsFromCents(double cents);

        /// Returns the height in cents of the curve of \a points at \a x milliseconds, with the
        /// segment that ends at each point drawn in the type of that point: the height of the
        /// first point before it, and of the last point after it. Unlike the curve that UTAU
        /// passes to the resampler, the value is not truncated to whole cents.
        ///
        /// \return 0 if \a points is empty.
        static double heightAt(const QList<PortamentoPoint> &points, double x);

        /// Returns the name of \a type in \c .usth, which is the name of the enumerator.
        static QString typeName(Type type);

        /// Returns the type named \a name in \c .usth, or \c std::nullopt if no type has this
        /// name.
        static std::optional<Type> typeFromName(QStringView name);

        QJsonObject toJson() const;

        /// Returns the point written as in \c .usth. An unknown curve type is reported in
        /// \a diagnostics and read as \c S.
        static PortamentoPoint fromJson(const QJsonObject &object, DiagnosticList &diagnostics);
    };

}

#endif // HELLOKIT_DOCUMENT_PORTAMENTOPOINT_H
