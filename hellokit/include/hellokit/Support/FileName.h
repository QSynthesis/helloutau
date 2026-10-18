#ifndef HELLOKIT_SUPPORT_FILENAME_H
#define HELLOKIT_SUPPORT_FILENAME_H

#include <filesystem>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// The form of the file names that a directory listing returns. See the section on the
    /// normalization of file names in docs/note.md.
    class HELLOKIT_SUPPORT_EXPORT FileName {
    public:
        /// Returns \a name, a file name of a directory listing, in the form that the model holds.
        /// On macOS the name is in Normalization Form C, as Qt reads it, because Qt writes names
        /// in Normalization Form D and the file systems of macOS find a file by either form. On
        /// other systems \a name is returned unchanged, because their file systems distinguish
        /// the two forms.
        static std::filesystem::path listed(const std::filesystem::path &name);
    };

}

#endif // HELLOKIT_SUPPORT_FILENAME_H
