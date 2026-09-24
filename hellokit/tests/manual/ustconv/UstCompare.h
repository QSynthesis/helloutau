#ifndef USTCONV_USTCOMPARE_H
#define USTCONV_USTCOMPARE_H

#include <functional>
#include <string>
#include <vector>

namespace utau {
    class UstFile;
}

namespace ustconv {

    /// One field whose value changed during a round trip.
    struct Difference {
        std::string where; ///< \c settings.tempo , or \c note 12 preUttr
        std::string before;
        std::string after;
    };

    /// Decodes the raw bytes of one file into text, the form in which the two sides are
    /// compared.
    using Normalizer = std::function<std::string(const std::string &)>;

    /// Compares the parse of a UST that was read with the parse of the UST written back.
    ///
    /// Values are compared as parsed rather than as written, and text as decoded rather than as
    /// encoded, which is the purpose of \a beforeText and \a afterText : the two files may use
    /// different encodings and different escaping, neither of which is a difference in the
    /// project.
    ///
    /// The control note is excluded on both sides. It marks a file as written by HelloUtau and
    /// is expected to appear, so counting it would report intended behavior as a defect.
    std::vector<Difference> compare(const utau::UstFile &before, const utau::UstFile &after,
                                    const Normalizer &beforeText, const Normalizer &afterText);

}

#endif // USTCONV_USTCOMPARE_H
