#include "Protocol.h"

#include <cstdio>

namespace fswatcher {

    std::string escape(std::string_view path) {
        std::string out;
        out.reserve(path.size());
        for (const char c : path) {
            switch (c) {
                case '%':
                    out += "%25";
                    break;
                case '\n':
                    out += "%0A";
                    break;
                case '\r':
                    out += "%0D";
                    break;
                default:
                    out += c;
                    break;
            }
        }
        return out;
    }

    std::string unescape(std::string_view text) {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '%' && i + 2 < text.size()) {
                const auto code = text.substr(i + 1, 2);
                if (code == "25") {
                    out += '%';
                    i += 2;
                    continue;
                }
                if (code == "0A") {
                    out += '\n';
                    i += 2;
                    continue;
                }
                if (code == "0D") {
                    out += '\r';
                    i += 2;
                    continue;
                }
            }
            out += text[i];
        }
        return out;
    }

    void Output::line(std::string_view word) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::fwrite(word.data(), 1, word.size(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }

    void Output::line(std::string_view word, std::string_view path) {
        std::string text(word);
        text += ' ';
        text += escape(path);
        line(text);
    }

}
