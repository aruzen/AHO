#include "support/vulkan_fixture.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

// heapとarenaの生成・破棄時間の比較。通常のテストでは実行しない。
//   vsl-test --gtest_also_run_disabled_tests --gtest_filter='*ArenaBenchmark*'
namespace {
    class ArenaBenchmark : public vsl_test::VulkanIntegrationTest {};

    using Clock = std::chrono::steady_clock;
    constexpr int REPEATS = 7;

    template<typename F>
    double median_us(F&& run) {
        std::vector<double> samples;
        for (int i = 0; i < REPEATS; ++i) {
            const auto start = Clock::now();
            run();
            samples.push_back(std::chrono::duration<double, std::micro>(Clock::now() - start).count());
        }
        std::ranges::sort(samples);
        return samples[samples.size() / 2];
    }

    void report(const char* name, std::size_t count, double heap, double arena) {
        std::printf("[ArenaBenchmark] %-28s n=%-6zu heap=%9.1fus arena=%9.1fus  arena/heap=%.2f\n", name, count, heap, arena, arena / heap);
    }

    // CPU側の確保だけを比較する。Vulkan呼び出しを含まない。
    TEST(ArenaCpuBenchmark, DISABLED_PlacementOnly) {
        struct ImplLike {
            std::shared_ptr<int> device;
            std::uint64_t handles[6]{};
        };
        constexpr std::size_t count = 10000;
        const auto heap = median_us([] {
            std::vector<std::shared_ptr<ImplLike>> objects;
            objects.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
                objects.push_back(vsl::place<ImplLike>(nullptr, [](ImplLike&) {}));
        });
        const auto arena = median_us([] {
            std::vector<std::shared_ptr<ImplLike>> objects;
            objects.reserve(count);
            vsl::Arena block(vsl::Arena::BytesFor<ImplLike>(count));
            for (std::size_t i = 0; i < count; ++i)
                objects.push_back(block.emplace<ImplLike>([](ImplLike&) {}));
        });
        report("cpu placement", count, heap, arena);
    }

    TEST_F(ArenaBenchmark, DISABLED_SamplerCreateDestroy) {
        constexpr std::size_t count = 1000;
        const auto heap = median_us([&] {
            std::vector<vsl::Sampler> samplers;
            samplers.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
                samplers.emplace_back(device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
        });
        const auto arena = median_us([&] {
            std::vector<vsl::Sampler> samplers;
            samplers.reserve(count);
            vsl::Arena block(vsl::Arena::BytesFor<vsl::impl::SamplerImpl>(count));
            for (std::size_t i = 0; i < count; ++i)
                samplers.emplace_back(block, device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
        });
        report("sampler create+destroy", count, heap, arena);
    }

    TEST_F(ArenaBenchmark, DISABLED_BufferCreateDestroy) {
        using HostBuffer = vsl::Buffer<vsl::MemoryType::UNIFORM_BUFFER, vsl::MemoryProperty::HOST_VISIBLE>;
        constexpr std::size_t count = 1000;
        const auto heap = median_us([&] {
            std::vector<HostBuffer> buffers;
            buffers.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
                buffers.emplace_back(device, 256);
        });
        const auto arena = median_us([&] {
            std::vector<HostBuffer> buffers;
            buffers.reserve(count);
            vsl::Arena block(vsl::Arena::BytesFor<vsl::impl::BufferImpl>(count));
            for (std::size_t i = 0; i < count; ++i)
                buffers.emplace_back(block, device, 256);
        });
        report("buffer create+destroy", count, heap, arena);
    }
} // namespace
