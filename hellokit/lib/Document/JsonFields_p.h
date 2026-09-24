#ifndef HELLOKIT_DOCUMENT_JSONFIELDS_P_H
#define HELLOKIT_DOCUMENT_JSONFIELDS_P_H

#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

namespace hello::kit {

    /// The reading and writing of the fields shared by the records of \c .usth.
    struct JsonFields {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::JsonFields)
    public:
        static inline void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message, std::nullopt});
        }

        static inline void complain(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Warning, message, std::nullopt});
        }

        /// Returns the number in the field \a key, or \c std::nullopt if the field is absent or
        /// null. A field of another type is reported in \a diagnostics and read as absent.
        static inline std::optional<double> readOptionalDouble(const QJsonObject &object,
                                                               const char *key,
                                                               DiagnosticList &diagnostics) {
            const auto value = object.value(QLatin1String(key));
            if (value.isUndefined() || value.isNull()) {
                return std::nullopt;
            }
            if (!value.isDouble()) {
                complain(diagnostics,
                         tr("\"%1\" is not a number and was omitted.").arg(QLatin1String(key)));
                return std::nullopt;
            }
            return value.toDouble();
        }

        static inline QString readString(const QJsonObject &object, const char *key) {
            return object.value(QLatin1String(key)).toString();
        }

        /// Writes \a value into the field \a key, or omits the field if \a value is empty. The
        /// format defines an omitted field and a null field as equivalent.
        static inline void writeOptionalDouble(QJsonObject &object, const char *key,
                                               const std::optional<double> &value) {
            if (value) {
                object.insert(QLatin1String(key), *value);
            }
        }
    };

}

#endif // HELLOKIT_DOCUMENT_JSONFIELDS_P_H
