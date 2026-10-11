#include "SynthFailure.h"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace hello::kit {

    void SynthFailure::readFile() {
        fileHead.clear();
        std::error_code ec;
        const auto size = std::filesystem::file_size(file, ec);
        if (ec) {
            fileSize = -1;
            return;
        }
        fileSize = qint64(size);
        std::ifstream in(file, std::ios::binary);
        fileHead.resize(qsizetype(std::min<std::uintmax_t>(size, headSize)));
        in.read(fileHead.data(), std::streamsize(fileHead.size()));
        fileHead.resize(qsizetype(in.gcount()));
    }

    bool SynthFailure::isWave(QByteArrayView head) {
        return head.size() >= 12 && head.first(4) == "RIFF" && head.sliced(8, 4) == "WAVE";
    }

}
