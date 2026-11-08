#pragma once
#include "../define.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace VSL_NAMESPACE {
    // 生成時の能力要求。10aでは有効化までで、Fence付きpresent／厳密回収は10cで接続する。
    enum class PresentFencePolicy {
        LEGACY,
        REQUIRE_FENCE
    };
    enum class DeviceFeature : std::uint64_t {
        NONE = 0,
        ROBUST_BUFFER_ACCESS = 1ull << 0,
        FULL_DRAW_INDEX_UINT32 = 1ull << 1,
        IMAGE_CUBE_ARRAY = 1ull << 2,
        INDEPENDENT_BLEND = 1ull << 3,
        GEOMETRY_SHADER = 1ull << 4,
        TESSELLATION_SHADER = 1ull << 5,
        SAMPLE_RATE_SHADING = 1ull << 6,
        DUAL_SRC_BLEND = 1ull << 7,
        LOGIC_OP = 1ull << 8,
        MULTI_DRAW_INDIRECT = 1ull << 9,
        DRAW_INDIRECT_FIRST_INSTANCE = 1ull << 10,
        DEPTH_CLAMP = 1ull << 11,
        DEPTH_BIAS_CLAMP = 1ull << 12,
        FILL_MODE_NON_SOLID = 1ull << 13,
        DEPTH_BOUNDS = 1ull << 14,
        WIDE_LINES = 1ull << 15,
        LARGE_POINTS = 1ull << 16,
        ALPHA_TO_ONE = 1ull << 17,
        MULTI_VIEWPORT = 1ull << 18,
        SAMPLER_ANISOTROPY = 1ull << 19,
        TEXTURE_COMPRESSION_ETC2 = 1ull << 20,
        TEXTURE_COMPRESSION_ASTC_LDR = 1ull << 21,
        TEXTURE_COMPRESSION_BC = 1ull << 22,
        OCCLUSION_QUERY_PRECISE = 1ull << 23,
        PIPELINE_STATISTICS_QUERY = 1ull << 24,
        VERTEX_PIPELINE_STORES_AND_ATOMICS = 1ull << 25,
        FRAGMENT_STORES_AND_ATOMICS = 1ull << 26,
        SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE = 1ull << 27,
        SHADER_IMAGE_GATHER_EXTENDED = 1ull << 28,
        SHADER_STORAGE_IMAGE_EXTENDED_FORMATS = 1ull << 29,
        SHADER_STORAGE_IMAGE_MULTISAMPLE = 1ull << 30,
        SHADER_STORAGE_IMAGE_READ_WITHOUT_FORMAT = 1ull << 31,
        SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT = 1ull << 32,
        SHADER_UNIFORM_BUFFER_ARRAY_DYNAMIC_INDEXING = 1ull << 33,
        SHADER_SAMPLED_IMAGE_ARRAY_DYNAMIC_INDEXING = 1ull << 34,
        SHADER_STORAGE_BUFFER_ARRAY_DYNAMIC_INDEXING = 1ull << 35,
        SHADER_STORAGE_IMAGE_ARRAY_DYNAMIC_INDEXING = 1ull << 36,
        SHADER_CLIP_DISTANCE = 1ull << 37,
        SHADER_CULL_DISTANCE = 1ull << 38,
        SHADER_FLOAT64 = 1ull << 39,
        SHADER_INT64 = 1ull << 40,
        SHADER_INT16 = 1ull << 41,
        SHADER_RESOURCE_RESIDENCY = 1ull << 42,
        SHADER_RESOURCE_MIN_LOD = 1ull << 43,
        SPARSE_BINDING = 1ull << 44,
        SPARSE_RESIDENCY_BUFFER = 1ull << 45,
        SPARSE_RESIDENCY_IMAGE2D = 1ull << 46,
        SPARSE_RESIDENCY_IMAGE3D = 1ull << 47,
        SPARSE_RESIDENCY2_SAMPLES = 1ull << 48,
        SPARSE_RESIDENCY4_SAMPLES = 1ull << 49,
        SPARSE_RESIDENCY8_SAMPLES = 1ull << 50,
        SPARSE_RESIDENCY16_SAMPLES = 1ull << 51,
        SPARSE_RESIDENCY_ALIASED = 1ull << 52,
        VARIABLE_MULTISAMPLE_RATE = 1ull << 53,
        INHERITED_QUERIES = 1ull << 54,
        TIMELINE_SEMAPHORE = 1ull << 55,
        SWAPCHAIN_MAINTENANCE1 = 1ull << 56
    };
    struct DeviceQueueRequest {
        std::uint32_t family = 0;
        std::vector<float> priorities{1.0f};
    };
    struct DeviceCreateInfo {
        // 空ならgraphics roleを選択し、必要なroleのqueue0を作る。
        std::vector<DeviceQueueRequest> queues;
        std::optional<std::uint32_t> graphics_family, present_family;
        std::vector<std::string> extensions;
        DeviceFeature features = DeviceFeature::NONE;
        bool require_compute = true;
    };
    constexpr DeviceFeature operator|(DeviceFeature a, DeviceFeature b) noexcept {
        return static_cast<DeviceFeature>(static_cast<std::uint64_t>(a) | static_cast<std::uint64_t>(b));
    }
    constexpr bool has_device_features(DeviceFeature features, DeviceFeature required) noexcept {
        return (static_cast<std::uint64_t>(features) & static_cast<std::uint64_t>(required)) == static_cast<std::uint64_t>(required);
    }
} // namespace VSL_NAMESPACE
