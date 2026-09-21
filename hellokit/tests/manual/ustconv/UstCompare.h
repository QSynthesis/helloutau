#ifndef USTCONV_USTCOMPARE_H
#define USTCONV_USTCOMPARE_H

#include <functional>
#include <string>
#include <vector>

namespace utau {
    class UstFile;
}

namespace ustconv {

    /// One field that came back from a round trip different from the way it went in.
    struct Difference {
        std::string where; ///< \c settings.tempo , or \c note 12 preUttr
        std::string before;
        std::string after;
    };

    /// Turns the raw bytes of one file into text, which is what the two sides are compared as.
    using Normalizer = std::function<std::string(const std::string &)>;

    /// Compares the parse of a UST that was read against the parse of the one written back.
    ///
    /// Values are compared as parsed rather than as spelled, and text as it reads rather than as
    /// it is encoded, which is what \a beforeText and \a afterText are for: the two files may be
    /// in different encodings and may escape differently, and neither is a difference in the
    /// project.
    ///
    /// The control note is left out of both sides. It is what marks a file as HelloUTAU's and is
    /// meant to appear, so counting it would report the design as a fault.
    std::vector<Difference> compare(const utau::UstFile &before, const utau::UstFile &after,
                                    const Normalizer &beforeText, const Normalizer &afterText);

}

#endif // USTCONV_USTCOMPARE_H
