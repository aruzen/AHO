#pragma once
#include "typed_buffer_arena.hpp"
#include "../utils/typed_resource_allocation.hpp"
#include <algorithm>

namespace VSL_NAMESPACE::typed {
    struct HeapStorage {};
    template<typename Bindings_ = BufferBindingMap<>>
    struct ArenaStorage {
        using Bindings = Bindings_;
    };

    struct BundleBufferPlacement {
        BindingKey binding;
        ResourceId resource;
        ResourceScope scope;
        bool external;
        size_t slot;
    };

    // binding一覧と、寿命群ごとの重複を除いた物理配置を分ける。
    template<typename Schema, typename Bindings = BufferBindingMap<>>
    class TypedBundlePlan {
        static consteval size_t BindingCount();
        template<size_t first, size_t second>
        static consteval bool SamePayload();
        static constexpr ResourceId Identity(size_t index);
        static constexpr auto KEYS = [] {
            std::array<BindingKey, BindingCount()> keys{};
            size_t index = 0;
            for (const auto& entry : Schema::ENTRIES)
                for (size_t element = 0; element < entry.descriptors; ++element)
                    keys[index++] = {entry.set, entry.binding, element};
            return keys;
        }();
        std::array<BundleBufferPlacement, BindingCount()> _placements{};
        std::array<BufferCreateInfo, BindingCount()> _shared{}, _frame{};
        size_t _shared_count = 0, _frame_count = 0;
        size_t _frames;

    public:
        constexpr explicit TypedBundlePlan(const ResourceAllocationPlan<Schema>& plan);
        constexpr const auto& placements() const { return _placements; }
        constexpr std::span<const BufferCreateInfo> buffers(ResourceScope scope) const;
        constexpr size_t bytes(ResourceScope scope) const { return storage::BufferLayout::Bytes(buffers(scope).size()); }
        constexpr size_t alignment(ResourceScope scope) const { return buffers(scope).empty() ? 1 : storage::BufferLayout::ALIGNMENT; }
        constexpr size_t total_bytes() const { return detail::allocation_add(bytes(ResourceScope::SHARED), detail::allocation_multiply(bytes(ResourceScope::PER_FRAME), _frames)); }
        constexpr const BundleBufferPlacement& placement(BindingKey key) const;
    };

    template<typename Schema, typename Bindings>
    consteval size_t TypedBundlePlan<Schema, Bindings>::BindingCount() {
        size_t count = 0;
        for (const auto& entry : Schema::ENTRIES)
            count = detail::allocation_add(count, entry.descriptors);
        return count;
    }
    template<typename Schema, typename Bindings>
    template<size_t first, size_t second>
    consteval bool TypedBundlePlan<Schema, Bindings>::SamePayload() {
        using A = std::tuple_element_t<first, typename Schema::Allocations>;
        using B = std::tuple_element_t<second, typename Schema::Allocations>;
        return std::is_same_v<typename Schema::template Binding<A::SET, A::BINDING>::Payload, typename Schema::template Binding<B::SET, B::BINDING>::Payload>;
    }
    template<typename Schema, typename Bindings>
    constexpr ResourceId TypedBundlePlan<Schema, Bindings>::Identity(size_t index) {
        for (size_t i = 0; i < Bindings::KEYS.size(); ++i)
            if (Bindings::KEYS[i] == KEYS[index])
                return Bindings::RESOURCES[i];
        // 自動IDは明示IDと衝突しない、宣言順の空き番号を使う。
        std::uint64_t id = 0;
        for (size_t current = 0; current <= index; ++current) {
            bool explicit_id = false;
            for (const auto key : Bindings::KEYS)
                explicit_id |= key == KEYS[current];
            if (explicit_id)
                continue;
            while (std::ranges::find(Bindings::RESOURCES, ResourceId{id}) != Bindings::RESOURCES.end()) {
                if (id == UINT64_MAX)
                    throw std::overflow_error("Resource ID overflow");
                ++id;
            }
            if (current == index)
                return ResourceId{id};
            if (id == UINT64_MAX)
                throw std::overflow_error("Resource ID overflow");
            ++id;
        }
        throw std::logic_error("Missing resource identity");
    }
    template<typename Schema, typename Bindings>
    constexpr TypedBundlePlan<Schema, Bindings>::TypedBundlePlan(const ResourceAllocationPlan<Schema>& plan)
        : _frames(plan.frame_count()) {
        constexpr auto payloads = [] {
            std::array<std::array<bool, Schema::COUNT>, Schema::COUNT> result{};
            [&]<size_t... i>(std::index_sequence<i...>) {
                auto row = [&]<size_t a, size_t... b>(std::index_sequence<b...>) { ((result[a][b] = SamePayload<a, b>()), ...); };
                (row.template operator()<i>(std::make_index_sequence<Schema::COUNT>{}), ...);
            }(std::make_index_sequence<Schema::COUNT>{});
            return result;
        }();
        for (const auto key : Bindings::KEYS)
            if (std::ranges::find(KEYS, key) == KEYS.end())
                throw std::invalid_argument("Unknown bundle binding key");
        size_t index = 0;
        for (size_t entry_index = 0; entry_index < Schema::COUNT; ++entry_index) {
            const auto& entry = plan.entries()[entry_index];
            const auto& declaration = entry.declaration;
            for (size_t element = 0; element < declaration.descriptors; ++element, ++index) {
                auto& target = _placements[index];
                target = {KEYS[index], Identity(index), declaration.scope, declaration.external, SIZE_MAX};
                const auto bytes = detail::allocation_multiply(entry.elements, declaration.payload_size);
                for (size_t previous = 0; previous < index; ++previous) {
                    const auto& other = _placements[previous];
                    if (other.resource != target.resource)
                        continue;
                    const auto other_index = Schema::Index(other.binding.set, other.binding.binding);
                    const auto& other_entry = plan.entries()[other_index];
                    if (target.external || other.external || target.scope != other.scope ||
                        declaration.kind != other_entry.declaration.kind || !payloads[entry_index][other_index] ||
                        entry.elements != other_entry.elements)
                        throw std::invalid_argument("Incompatible declarations for one bundle resource ID");
                    target.slot = other.slot;
                    break;
                }
                if (target.external || target.slot != SIZE_MAX)
                    continue;
                auto& count = target.scope == ResourceScope::SHARED ? _shared_count : _frame_count;
                auto& buffers = target.scope == ResourceScope::SHARED ? _shared : _frame;
                target.slot = count++;
                const auto usage = declaration.kind == graphic_resource::Type::UNIFORM_BUFFER ? MemoryType::UNIFORM_BUFFER : MemoryType::STORAGE_BUFFER;
                buffers[target.slot] = {bytes, usage | MemoryType::TRANSFER_DESTINATION, MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT};
            }
        }
        static_cast<void>(total_bytes());
    }
    template<typename Schema, typename Bindings>
    constexpr std::span<const BufferCreateInfo> TypedBundlePlan<Schema, Bindings>::buffers(ResourceScope scope) const {
        if (scope == ResourceScope::SHARED)
            return {_shared.data(), _shared_count};
        if (scope == ResourceScope::PER_FRAME)
            return {_frame.data(), _frame_count};
        throw std::invalid_argument("Unknown resource scope");
    }
    template<typename Schema, typename Bindings>
    constexpr const BundleBufferPlacement& TypedBundlePlan<Schema, Bindings>::placement(BindingKey key) const {
        for (const auto& entry : _placements)
            if (entry.binding == key)
                return entry;
        throw std::invalid_argument("Unknown bundle binding key");
    }
} // namespace VSL_NAMESPACE::typed
