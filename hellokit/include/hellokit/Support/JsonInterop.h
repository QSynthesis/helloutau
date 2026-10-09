#ifndef HELLOKIT_SUPPORT_JSONINTEROP_H
#define HELLOKIT_SUPPORT_JSONINTEROP_H

#include <string_view>

#include <QtCore/QJsonValue>

#include <stdcorelib/support/json.h>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// Conversion between the JSON values of Qt and of stdcorelib, and access to a value in
    /// nested JSON objects by its path.
    ///
    /// The values of stdcorelib can be modified in place, which suits a document that is held in
    /// memory and changed by path, such as a settings file. Qt code uses the values of Qt.
    ///
    /// A path names a value in the groups of an object, with the names joined by slashes, such
    /// as <tt>synthTools/resampler</tt>.
    class HELLOKIT_SUPPORT_EXPORT JsonInterop {
    public:
        /// Converts \a value to a stdcorelib value. A number without a fractional part that
        /// fits an integer becomes an integer, as Qt stores it. Undefined becomes null.
        static stdc::json::Value fromQtJson(const QJsonValue &value);

        /// Converts \a value to a Qt value. Binary data has no Qt counterpart and becomes null.
        static QJsonValue toQtJson(const stdc::json::Value &value);

        /// Returns the value at \a path in \a object, or null if absent.
        static const stdc::json::Value &valueAt(const stdc::json::Object &object,
                                                std::string_view path);

        /// Replaces the value at \a path in \a object and creates the enclosing groups. If
        /// \a value is null, removes the value together with each group that the removal leaves
        /// empty.
        static void insertAt(stdc::json::Object &object, std::string_view path,
                             stdc::json::Value value);
    };

}

#endif // HELLOKIT_SUPPORT_JSONINTEROP_H
