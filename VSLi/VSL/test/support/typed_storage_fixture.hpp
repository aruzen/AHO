#pragma once
#include <VSL/utils/typed_resource_bundle.hpp>
#include <VSL/utils/typed_compute.hpp>
#include "vulkan_fixture.hpp"

namespace vsl_test::storage_policy {
    namespace t = vsl::typed;
    using LocalSet = t::DescriptorSet<2, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                      t::UniformBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                      t::StorageBinding<3, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
    using SharedSet = t::DescriptorSet<5, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE, 2>>;
    using Resources = t::ResourceSchema<LocalSet, SharedSet>;
    using Allocation = t::ResourceAllocationSchema<Resources, t::BufferAllocation<2, 0>,
                                                   t::BufferAllocation<2, 1, t::FixedElements<1>, t::ResourceScope::SHARED>,
                                                   t::BufferAllocation<2, 3, t::RuntimeElements>,
                                                   t::BufferAllocation<5, 0, t::FixedElements<1>, t::ResourceScope::SHARED>>;
    struct Call {
        std::uint32_t set, binding;
        size_t element, count;
    };
    struct Counters {
        std::vector<Call> calls;
        std::vector<std::weak_ptr<void>> owners;
        size_t maps = 0, fail_after = SIZE_MAX;
    };
    inline t::ResourceAllocationSizes<Allocation> sizes() {
        t::ResourceAllocationSizes<Allocation> value;
        value.set<2, 3>(4);
        return value;
    }
    template<typename Bundle>
    void initialize_shared(Bundle& bundle) {
        bundle.initialize_shared([](typename Bundle::Shared& shared) {
            const std::uint32_t bias = 10, first = 20, second = 30;
            shared.template buffer<2, 1>().copy(std::span(&bias, 1));
            shared.template buffer<5, 0, 0>().copy(std::span(&first, 1));
            shared.template buffer<5, 0, 1>().copy(std::span(&second, 1));
        });
    }
    inline void host_read_barrier(vsl::CommandManager manager, std::uint32_t slot) {
        auto buffer = manager.get_buffer();
        vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        buffer.data_->command_buffers.at(slot).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
    }
} // namespace vsl_test::storage_policy
