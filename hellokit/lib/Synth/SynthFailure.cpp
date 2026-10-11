#include "SynthFailure.h"

#include <fstream>
#include <system_error>

namespace hello::kit {

    void SynthFailure::readOutput() {
        outputHead.clear();
        std::error_code ec;
        const auto size = std::filesystem::file_size(outputFile, ec);
        if (ec) {
            outputSize = -1;
            return;
        }
        outputSize = qint64(size);
        std::ifstream in(outputFile, std::ios::binary);
        outputHead.resize(qsizetype(std::min<std::uintmax_t>(size, headSize)));
        in.read(outputHead.data(), std::streamsize(outputHead.size()));
        outputHead.resize(qsizetype(in.gcount()));
    }

    bool SynthFailure::isWave(QByteArrayView head) {
        return head.size() >= 12 && head.first(4) == "RIFF" && head.sliced(8, 4) == "WAVE";
    }

}
