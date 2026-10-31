#include <VSL/storage/cpu_region.hpp>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <algorithm>

namespace VSL_NAMESPACE::storage {
    namespace detail {
        void validate_cpu_alignment(size_t alignment);
        void validate_cpu_alignment(size_t alignment) {
            if (!alignment || (alignment & (alignment - 1)))
                throw std::invalid_argument("CPU storage alignment must be a power of two");
        }
        CPURegion::CPURegion(size_t bytes, size_t alignment) : bytes(bytes), alignment(alignment), allocated(true) {
            validate_cpu_alignment(alignment);
            if (bytes)
                base = static_cast<std::byte*>(::operator new(bytes, std::align_val_t(std::max(alignment, alignof(void*)))));
        }
        CPURegion::CPURegion(std::span<std::byte> region, size_t alignment, std::shared_ptr<void> owner)
            : external_owner(std::move(owner)), base(region.data()), bytes(region.size()), alignment(alignment) {
            validate_cpu_alignment(alignment);
            if (bytes && (!base || !external_owner.use_count() || reinterpret_cast<std::uintptr_t>(base) % alignment))
                throw std::invalid_argument("CPU storage owner or address alignment mismatch");
        }
        CPURegion::~CPURegion() {
            if (allocated)
                ::operator delete(base, std::align_val_t(std::max(alignment, alignof(void*))));
        }
        void CPURegion::claim(size_t required_bytes, size_t required_alignment) {
            if (claimed || bytes < required_bytes || (required_bytes && (alignment < required_alignment || !base || reinterpret_cast<std::uintptr_t>(base) % required_alignment)))
                throw std::invalid_argument("CPU region is in use, too small or misaligned");
            claimed = true;
        }
    } // namespace detail
    ArenaMemory::ArenaMemory(size_t bytes, size_t alignment) : _region(std::make_shared<detail::CPURegion>(bytes, alignment)) {}
    ArenaMemory::ArenaMemory(std::span<std::byte> region, size_t alignment, std::shared_ptr<void> owner)
        : _region(std::make_shared<detail::CPURegion>(region, alignment, std::move(owner))) {}
    size_t ArenaMemory::size() const {
        return _region ? _region->bytes : 0;
    }
    size_t ArenaMemory::alignment() const {
        return _region ? _region->alignment : 1;
    }
} // namespace VSL_NAMESPACE::storage
