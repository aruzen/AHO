#pragma once
#include <AHO/define.hpp>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace AHO_NAMESPACE::x::resource {
    // glTF 2.0はmaterialを保存し、shader本体は含めない。外部のSPIR-VをCPUへ読む。
    // little-endian SPIR-V。ヘッダー・命令長を検証するが、shader意味論のvalidatorではない。
    // GPU module作成は既存VSL::Shaderへ元のpathを渡す。
    std::vector<std::uint32_t> loadSpirv(const std::filesystem::path& path);
} // namespace AHO_NAMESPACE::x::resource
