#include <AHO/x/resource/shader.hpp>
#include <bit>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace AHO_NAMESPACE::x::resource {
    std::vector<std::uint32_t> loadSpirv(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            throw std::runtime_error("Could not open SPIR-V: " + path.string());
        const auto size = file.tellg();
        if (size < 20 || size % 4 != 0 || static_cast<std::uintmax_t>(size) > std::numeric_limits<std::size_t>::max() ||
            static_cast<std::uintmax_t>(size) > std::numeric_limits<std::streamsize>::max())
            throw std::runtime_error("Invalid SPIR-V size: " + path.string());
        std::vector<std::uint32_t> words(static_cast<std::size_t>(size) / sizeof(std::uint32_t));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(words.data()), static_cast<std::streamsize>(size)))
            throw std::runtime_error("Could not read SPIR-V: " + path.string());
        if constexpr (std::endian::native == std::endian::big)
            for (auto& word : words)
                word = std::byteswap(word);
        if (words[0] != 0x07230203 || (words[1] & 0xff0000ffu) || words[1] < 0x00010000 || words[1] > 0x00010600 ||
            !words[3] || words[4] != 0)
            throw std::runtime_error("Invalid SPIR-V header: " + path.string());
        for (std::size_t i = 5; i < words.size();) {
            const auto count = words[i] >> 16;
            if (!count || count > words.size() - i)
                throw std::runtime_error("Invalid SPIR-V instruction length: " + path.string());
            i += count;
        }
        return words;
    }
} // namespace AHO_NAMESPACE::x::resource
