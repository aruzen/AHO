#pragma once
#include <AHO/resource/buffer.hpp>

namespace aho_test {
    struct CustomCounters {
        unsigned maps = 0, allocations = 0;
    };

    using NativeHost = vsl::Buffer<vsl::MemoryType::TransferSource | vsl::MemoryType::TransferDestination, vsl::MemoryProperty::HostVisible>;
    struct CountingCustomization {
        vsl::LogicalDeviceAccessor device;
        std::shared_ptr<CustomCounters> counters;
        auto map(NativeHost& storage);
        NativeHost reallocate(const NativeHost&, std::size_t size);
    };

    inline auto CountingCustomization::map(NativeHost& storage) {
        ++counters->maps;
        return storage.data();
    }

    inline NativeHost CountingCustomization::reallocate(const NativeHost&, std::size_t size) {
        ++counters->allocations;
        return NativeHost(device, size);
    }
} // namespace aho_test
