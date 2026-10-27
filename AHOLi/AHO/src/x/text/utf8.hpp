#pragma once
#include <AHO/define.hpp>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace AHO_NAMESPACE::x::detail {
    struct DecodedCodepoint {
        char32_t value;
        std::size_t offset;
    };
    inline void require_scalar(char32_t value);
    inline std::vector<DecodedCodepoint> decode_utf8(std::string_view text);

    inline void require_scalar(char32_t value) {
        if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            throw std::invalid_argument("Text requires Unicode scalar values");
    }

    inline std::vector<DecodedCodepoint> decode_utf8(std::string_view text) {
        std::vector<DecodedCodepoint> result;
        for (std::size_t i = 0; i < text.size();) {
            const auto offset = i;
            const auto lead = static_cast<unsigned char>(text[i++]);
            char32_t value = lead;
            unsigned remaining = 0;
            char32_t minimum = 0;
            if (lead < 0x80) {
            } else if (lead >= 0xc2 && lead <= 0xdf) {
                value = lead & 0x1f;
                remaining = 1;
                minimum = 0x80;
            } else if (lead >= 0xe0 && lead <= 0xef) {
                value = lead & 0xf;
                remaining = 2;
                minimum = 0x800;
            } else if (lead >= 0xf0 && lead <= 0xf4) {
                value = lead & 7;
                remaining = 3;
                minimum = 0x10000;
            } else
                throw std::invalid_argument("Invalid UTF-8 leading byte");
            if (remaining > text.size() - i)
                throw std::invalid_argument("Truncated UTF-8 sequence");
            for (unsigned j = 0; j < remaining; ++j) {
                const auto next = static_cast<unsigned char>(text[i++]);
                if ((next & 0xc0) != 0x80)
                    throw std::invalid_argument("Invalid UTF-8 continuation byte");
                value = (value << 6) | (next & 0x3f);
            }
            if (value < minimum)
                throw std::invalid_argument("Overlong UTF-8 sequence");
            require_scalar(value);
            result.push_back({value, offset});
        }
        return result;
    }
} // namespace AHO_NAMESPACE::x::detail
