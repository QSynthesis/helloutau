#include "PayloadCodec.h"

#include <array>

namespace hu {

    static constexpr const char ALPHABET[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    // 0xFF for everything the alphabet does not cover, which is what decode() turns away on.
    static constexpr std::array<unsigned char, 256> buildReverse() {
        std::array<unsigned char, 256> table{};
        for (auto &entry : table) {
            entry = 0xFF;
        }
        for (unsigned char i = 0; i < 64; ++i) {
            table[static_cast<unsigned char>(ALPHABET[i])] = i;
        }
        return table;
    }

    std::string PayloadCodec::encode(std::string_view data) {
        std::string out;
        out.reserve((data.size() + 2) / 3 * 4);

        std::size_t i = 0;
        while (i + 3 <= data.size()) {
            auto group =
                (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << 16) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i + 1])) << 8) |
                static_cast<std::uint32_t>(static_cast<unsigned char>(data[i + 2]));
            out += ALPHABET[(group >> 18) & 0x3F];
            out += ALPHABET[(group >> 12) & 0x3F];
            out += ALPHABET[(group >> 6) & 0x3F];
            out += ALPHABET[group & 0x3F];
            i += 3;
        }

        // The tail goes out as the two or three characters it fills, with no padding to round it
        // up. An equals sign would be truncated by UTAU, so there is nowhere for it to go.
        auto rest = data.size() - i;
        if (rest == 1) {
            auto group = static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << 16;
            out += ALPHABET[(group >> 18) & 0x3F];
            out += ALPHABET[(group >> 12) & 0x3F];
        } else if (rest == 2) {
            auto group = (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << 16) |
                         (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i + 1])) << 8);
            out += ALPHABET[(group >> 18) & 0x3F];
            out += ALPHABET[(group >> 12) & 0x3F];
            out += ALPHABET[(group >> 6) & 0x3F];
        }
        return out;
    }

    std::optional<std::string> PayloadCodec::decode(std::string_view text) {
        static constexpr auto reverse = buildReverse();

        // One character on its own encodes six bits, which is not enough to have come from a byte.
        if (text.size() % 4 == 1) {
            return std::nullopt;
        }

        std::string out;
        out.reserve(text.size() / 4 * 3);

        std::uint32_t group = 0;
        int filled = 0;
        for (char c : text) {
            auto value = reverse[static_cast<unsigned char>(c)];
            if (value == 0xFF) {
                return std::nullopt;
            }

            group = (group << 6) | value;
            filled++;
            if (filled == 4) {
                out += static_cast<char>((group >> 16) & 0xFF);
                out += static_cast<char>((group >> 8) & 0xFF);
                out += static_cast<char>(group & 0xFF);
                group = 0;
                filled = 0;
            }
        }

        if (filled == 2) {
            out += static_cast<char>((group >> 4) & 0xFF);
        } else if (filled == 3) {
            out += static_cast<char>((group >> 10) & 0xFF);
            out += static_cast<char>((group >> 2) & 0xFF);
        }
        return out;
    }

}
