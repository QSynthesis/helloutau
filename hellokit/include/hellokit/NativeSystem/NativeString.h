#ifndef HELLOKIT_NATIVESYSTEM_NATIVESTRING_H
#define HELLOKIT_NATIVESYSTEM_NATIVESTRING_H

#include <string>

/// A string literal of the type of NativeString: \a text with the prefix L on Windows, and
/// unchanged elsewhere.
#ifdef _WIN32
#  define HELLOKIT_NATIVE(text) L##text
#else
#  define HELLOKIT_NATIVE(text) text
#endif

namespace hello::kit {

    /// The string of the system interfaces for paths, arguments and the environment: UTF-16 in
    /// \c std::wstring on Windows, and bytes in \c std::string elsewhere, which are UTF-8 in the
    /// locales that HelloUtau supports. The type equals \c std::filesystem::path::string_type .
    ///
    /// The library HelloKitNativeSystem serves the programs that link no Qt, such as
    /// hello-updater and hello-plugin-play. It is static and, on Windows, links the static C
    /// runtime, as these programs do.
#ifdef _WIN32
    using NativeString = std::wstring;
#else
    using NativeString = std::string;
#endif

    /// Returns \a text in UTF-8. On Windows an unpaired surrogate becomes U+FFFD. Elsewhere the
    /// bytes are returned unchanged.
    std::string utf8Of(const NativeString &text);

    /// Returns \a text , which is UTF-8, as a native string. On Windows an invalid byte sequence
    /// becomes U+FFFD. Elsewhere the bytes are returned unchanged.
    NativeString nativeOf(const std::string &text);

}

#endif // HELLOKIT_NATIVESYSTEM_NATIVESTRING_H
