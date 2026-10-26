#pragma once
#include "typed_descriptor_schema.hpp"
#include <limits>

namespace VSL_NAMESPACE::typed {
    enum class ResourceScope {
        SHARED,
        PER_FRAME
    };

    template<size_t count>
    struct FixedElements {
        static_assert(count > 0, "Typed allocation element count must be positive");
        static constexpr size_t COUNT = count;
    };
    struct RuntimeElements {};

    namespace detail {
        template<typename T>
        struct AllocationElements {
            static constexpr bool VALID = false, DYNAMIC = false;
            static constexpr size_t COUNT = 0;
        };
        template<size_t count>
        struct AllocationElements<FixedElements<count>> {
            static constexpr bool VALID = count > 0, DYNAMIC = false;
            static constexpr size_t COUNT = count;
        };
        template<>
        struct AllocationElements<RuntimeElements> {
            static constexpr bool VALID = true, DYNAMIC = true;
            static constexpr size_t COUNT = 0;
        };
        template<std::uint32_t set, std::uint32_t binding, typename... Sets>
        struct AllocationBinding;
        template<std::uint32_t set, std::uint32_t binding>
        struct AllocationBinding<set, binding> {
            using Type = void;
        };
        template<std::uint32_t set, std::uint32_t binding, typename First, typename... Rest>
        struct AllocationBinding<set, binding, First, Rest...> {
            using Type = std::conditional_t<First::SET == set, typename First::template Binding<binding>, typename AllocationBinding<set, binding, Rest...>::Type>;
        };
        template<typename Tuple, std::uint32_t set, std::uint32_t binding>
        struct SchemaAllocationBinding;
        template<typename... Sets, std::uint32_t set, std::uint32_t binding>
        struct SchemaAllocationBinding<std::tuple<Sets...>, set, binding> : AllocationBinding<set, binding, Sets...> {};
        template<std::uint32_t set, std::uint32_t binding, typename... Specs>
        struct FindAllocation;
        template<std::uint32_t set, std::uint32_t binding>
        struct FindAllocation<set, binding> {
            using Type = void;
        };
        template<std::uint32_t set, std::uint32_t binding, typename First, typename... Rest>
        struct FindAllocation<set, binding, First, Rest...> {
            using Type = std::conditional_t<First::SET == set && First::BINDING == binding, First, typename FindAllocation<set, binding, Rest...>::Type>;
        };
        constexpr size_t allocation_add(size_t first, size_t second);
        constexpr size_t allocation_multiply(size_t first, size_t second);
    } // namespace detail

    template<std::uint32_t set, std::uint32_t binding, typename Elements = FixedElements<1>, ResourceScope scope = ResourceScope::PER_FRAME>
    struct BufferAllocation {
        static_assert(detail::AllocationElements<Elements>::VALID, "Invalid typed allocation element policy");
        static constexpr std::uint32_t SET = set, BINDING = binding;
        static constexpr ResourceScope SCOPE = scope;
        static constexpr bool EXTERNAL = false, DYNAMIC = detail::AllocationElements<Elements>::DYNAMIC;
        static constexpr size_t ELEMENTS = detail::AllocationElements<Elements>::COUNT;
    };
    template<std::uint32_t set, std::uint32_t binding>
    struct ExternalAllocation {
        static constexpr std::uint32_t SET = set, BINDING = binding;
        static constexpr ResourceScope SCOPE = ResourceScope::SHARED;
        static constexpr bool EXTERNAL = true, DYNAMIC = false;
        static constexpr size_t ELEMENTS = 0;
    };

    struct ResourceAllocationEntry {
        std::uint32_t set, binding;
        graphic_resource::Type kind;
        std::uint32_t descriptors;
        size_t payload_size;
        ResourceScope scope;
        bool external, dynamic;
        size_t elements;
    };

    template<typename Resources_, typename... Allocations_>
    struct ResourceAllocationSchema {
        using Resources = Resources_;
        using Allocations = std::tuple<Allocations_...>;
        static constexpr size_t COUNT = sizeof...(Allocations_);
        template<std::uint32_t set, std::uint32_t binding>
        using Spec = typename detail::FindAllocation<set, binding, Allocations_...>::Type;
        template<std::uint32_t set, std::uint32_t binding>
        using Binding = typename detail::SchemaAllocationBinding<typename Resources::Sets, set, binding>::Type;
        static consteval auto MakeEntries();
        static constexpr auto ENTRIES = MakeEntries();
        static consteval bool IsValid();
        static_assert(IsValid(), "Typed allocation must cover every shader binding exactly once with a valid allocation kind/scope");
        static constexpr size_t Index(std::uint32_t set, std::uint32_t binding);
        static constexpr size_t SetIndex(std::uint32_t set);
        static constexpr bool SetIsPerFrame(std::uint32_t set);
        template<std::uint32_t set, std::uint32_t binding>
        static consteval bool IsDynamic();
    };

    template<typename Schema>
    class ResourceAllocationSizes {
        std::array<size_t, Schema::COUNT> _values{};

    public:
        constexpr ResourceAllocationSizes();
        template<std::uint32_t set_number, std::uint32_t binding>
        requires(Schema::template IsDynamic<set_number, binding>())
        constexpr ResourceAllocationSizes& set(size_t elements);
        constexpr const auto& values() const { return _values; }
    };

    struct ResourceAllocationRecord {
        ResourceAllocationEntry declaration;
        size_t elements, instances, buffers, payload_bytes;
    };
    struct ResourceAllocationTotals {
        size_t buffers = 0, descriptor_sets = 0, descriptors = 0, payload_bytes = 0;
    };
    template<typename Schema_>
    class ResourceAllocationPlan {
        std::array<ResourceAllocationRecord, Schema_::COUNT> _entries{};
        ResourceAllocationTotals _totals;
        size_t _frames;

    public:
        using Schema = Schema_;
        constexpr explicit ResourceAllocationPlan(size_t frames, const ResourceAllocationSizes<Schema>& sizes = {});
        constexpr size_t frame_count() const { return _frames; }
        constexpr const auto& entries() const { return _entries; }
        constexpr const auto& totals() const { return _totals; }
        template<std::uint32_t set, std::uint32_t binding>
        requires(!std::is_void_v<typename Schema::template Spec<set, binding>>)
        constexpr const ResourceAllocationRecord& entry() const { return _entries[Schema::Index(set, binding)]; }
    };

    namespace detail {
        constexpr size_t allocation_add(size_t first, size_t second) {
            if (second > std::numeric_limits<size_t>::max() - first)
                throw std::overflow_error("Typed allocation total overflow");
            return first + second;
        }
        constexpr size_t allocation_multiply(size_t first, size_t second) {
            if (first && second > std::numeric_limits<size_t>::max() / first)
                throw std::overflow_error("Typed allocation size overflow");
            return first * second;
        }
    } // namespace detail

    template<typename Resources, typename... Specs>
    consteval auto ResourceAllocationSchema<Resources, Specs...>::MakeEntries() {
        std::array<ResourceAllocationEntry, COUNT> entries{};
        size_t index = 0;
        auto add = [&]<typename Spec>() {
            auto& entry = entries[index++];
            entry.set = Spec::SET;
            entry.binding = Spec::BINDING;
            entry.scope = Spec::SCOPE;
            entry.external = Spec::EXTERNAL;
            entry.dynamic = Spec::DYNAMIC;
            entry.elements = Spec::ELEMENTS;
            for (const auto& requirement : Resources::REQUIREMENTS)
                if (requirement.set == Spec::SET && requirement.point.binding == Spec::BINDING) {
                    entry.kind = requirement.point.binding_type;
                    entry.descriptors = requirement.point.amount;
                    entry.payload_size = requirement.payload_size;
                }
        };
        (add.template operator()<Specs>(), ...);
        return entries;
    }

    template<typename Resources, typename... Specs>
    consteval bool ResourceAllocationSchema<Resources, Specs...>::IsValid() {
        if (COUNT != Resources::REQUIREMENTS.size())
            return false;
        for (size_t i = 0; i < COUNT; ++i) {
            const auto& entry = ENTRIES[i];
            if (!entry.descriptors || (entry.scope != ResourceScope::SHARED && entry.scope != ResourceScope::PER_FRAME))
                return false;
            if (!entry.external && (entry.kind != graphic_resource::Type::UNIFORM_BUFFER && entry.kind != graphic_resource::Type::STORAGE_BUFFER))
                return false;
            for (size_t j = 0; j < i; ++j)
                if (entry.set == ENTRIES[j].set && entry.binding == ENTRIES[j].binding)
                    return false;
        }
        return true;
    }

    template<typename Resources, typename... Specs>
    constexpr size_t ResourceAllocationSchema<Resources, Specs...>::Index(std::uint32_t set, std::uint32_t binding) {
        for (size_t index = 0; index < COUNT; ++index)
            if (ENTRIES[index].set == set && ENTRIES[index].binding == binding)
                return index;
        throw std::invalid_argument("Unknown typed allocation binding");
    }
    template<typename Resources, typename... Specs>
    constexpr size_t ResourceAllocationSchema<Resources, Specs...>::SetIndex(std::uint32_t set) {
        for (size_t index = 0; index < Resources::COUNT; ++index)
            if (Resources::SET_NUMBERS[index] == set)
                return index;
        throw std::invalid_argument("Unknown typed allocation set");
    }
    template<typename Resources, typename... Specs>
    constexpr bool ResourceAllocationSchema<Resources, Specs...>::SetIsPerFrame(std::uint32_t set) {
        for (const auto& entry : ENTRIES)
            if (entry.set == set && entry.scope == ResourceScope::PER_FRAME)
                return true;
        return false;
    }
    template<typename Resources, typename... Specs>
    template<std::uint32_t set, std::uint32_t binding>
    consteval bool ResourceAllocationSchema<Resources, Specs...>::IsDynamic() {
        using Allocation = Spec<set, binding>;
        if constexpr (std::is_void_v<Allocation>)
            return false;
        else
            return Allocation::DYNAMIC;
    }

    template<typename Schema>
    constexpr ResourceAllocationSizes<Schema>::ResourceAllocationSizes() {
        for (size_t index = 0; index < Schema::COUNT; ++index)
            _values[index] = Schema::ENTRIES[index].elements;
    }
    template<typename Schema>
    template<std::uint32_t set_number, std::uint32_t binding>
    requires(Schema::template IsDynamic<set_number, binding>())
    constexpr ResourceAllocationSizes<Schema>& ResourceAllocationSizes<Schema>::set(size_t elements) {
        if (!elements)
            throw std::invalid_argument("Typed allocation runtime element count must be positive");
        _values[Schema::Index(set_number, binding)] = elements;
        return *this;
    }
    template<typename Schema>
    constexpr ResourceAllocationPlan<Schema>::ResourceAllocationPlan(size_t frames, const ResourceAllocationSizes<Schema>& sizes)
        : _frames(frames) {
        if (!frames)
            throw std::invalid_argument("Typed allocation frame count must be positive");
        for (size_t index = 0; index < Schema::COUNT; ++index) {
            const auto& entry = Schema::ENTRIES[index];
            const auto elements = sizes.values()[index];
            if (!entry.external && !elements)
                throw std::invalid_argument("Typed allocation runtime element count was not supplied");
            const auto instances = entry.scope == ResourceScope::PER_FRAME ? frames : 1;
            const auto buffers = entry.external ? 0 : detail::allocation_multiply(entry.descriptors, instances);
            const auto bytes = detail::allocation_multiply(detail::allocation_multiply(elements, entry.payload_size), buffers);
            _entries[index] = {entry, elements, instances, buffers, bytes};
            _totals.buffers = detail::allocation_add(_totals.buffers, buffers);
            _totals.payload_bytes = detail::allocation_add(_totals.payload_bytes, bytes);
        }
        for (const auto set : Schema::Resources::SET_NUMBERS) {
            const auto instances = Schema::SetIsPerFrame(set) ? frames : 1;
            _totals.descriptor_sets = detail::allocation_add(_totals.descriptor_sets, instances);
            for (const auto& entry : Schema::ENTRIES)
                if (entry.set == set)
                    _totals.descriptors = detail::allocation_add(_totals.descriptors, detail::allocation_multiply(entry.descriptors, instances));
        }
    }
} // namespace VSL_NAMESPACE::typed
