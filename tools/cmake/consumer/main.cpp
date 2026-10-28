#include <AHO/core/point.hpp>
#include <AHO/resource/buffer.hpp>
#include <VSL/utils/spir_reflector.hpp>
#include <array>
#include <cstddef>

int main() {
    aho::resource::Buffer<aho::resource::BufferType::GENERAL> buffer;
    const std::array bytes{std::byte{42}};
    buffer.assign(bytes);
    if (buffer.size() != bytes.size() || buffer.bytes().front() != bytes.front())
        return 1;

    try {
        vsl::utils::SPIRVReflector reflector({}, std::vector<char>{});
    } catch (const vsl::exceptions::RuntimeException&) {
        return 0;
    }
    return 2;
}
