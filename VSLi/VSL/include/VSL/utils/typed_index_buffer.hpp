#pragma once
#include "../vulkan/buffer.hpp"
#include "typed_buffer_view.hpp"
#include "../vulkan/resource_use.hpp"
#include "../vulkan/commands/bind_index_buffers.hpp"
#include <algorithm>
#include <concepts>
#include <span>
#include <stdexcept>
#include <limits>

namespace VSL_NAMESPACE::typed {
    template<typename T>
    concept index_element = std::same_as<T, std::uint16_t> || std::same_as<T, std::uint32_t>;

    namespace detail {
        template<index_element Index>
        struct IndexSnapshot {
            std::vector<Index> values;
            std::uint64_t revision = 0;
            bool valid = true;
            explicit IndexSnapshot(std::span<const Index> values) : values(values.begin(), values.end()) {}
        };
    } // namespace detail

    template<index_element Index_, MemoryProperty property = MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT>
    class TypedIndexBuffer {
    public:
        using Index = Index_;
        static constexpr command::IndexType INDEX_TYPE = sizeof(Index) == 2 ? command::IndexType::UINT16 : command::IndexType::UINT32;

    private:
        template<typename, typename, typename, bool, bool>
        friend class TypedVertexBinding;
        static constexpr auto USAGE = MemoryType::INDEX_BUFFER | MemoryType::TRANSFER_DESTINATION;
        static std::shared_ptr<detail::IndexSnapshot<Index>> MakeSnapshot(std::span<const Index> values);
        std::shared_ptr<detail::IndexSnapshot<Index>> _values;
        BufferView _buffer;

    public:
        template<buffer_storage_policy Policy = DefaultStorage>
        TypedIndexBuffer(LogicalDeviceAccessor device, std::span<const Index> values, Policy policy = {})
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        template<buffer_storage_policy Policy = DefaultStorage>
        TypedIndexBuffer(LogicalDeviceAccessor device, CommandManager manager, std::span<const Index> values, Policy policy = {})
        requires(!contain<property, MemoryProperty::HOST_VISIBLE>());
        size_t size() const { return _values->values.size(); }
        LogicalDeviceAccessor device() const { return _buffer.ref().device(); }
        ResourceUse resource_use() const { return _buffer.resource_use(); }
        const BufferView& backend() const& { return _buffer; }
        const BufferView& backend() const&& = delete;
        BufferView storage_view() const { return _buffer; }
        void copy(std::span<const Index> values)
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        std::pair<std::uint32_t, std::uint32_t> bounds(size_t first, size_t count) const;
    };

    template<index_element Index, MemoryProperty property>
    std::shared_ptr<detail::IndexSnapshot<Index>> TypedIndexBuffer<Index, property>::MakeSnapshot(std::span<const Index> values) {
        (void)helper::checked_buffer_array_size<Index>(values.size());
        return std::make_shared<detail::IndexSnapshot<Index>>(values);
    }
    template<index_element Index, MemoryProperty property>
    template<buffer_storage_policy Policy>
    TypedIndexBuffer<Index, property>::TypedIndexBuffer(LogicalDeviceAccessor device, std::span<const Index> values, Policy policy)
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
        : _values(MakeSnapshot(values)), _buffer(detail::make_buffer_storage(device, {helper::checked_buffer_array_size<Index>(_values->values.size()), USAGE, property}, policy)) {
        _buffer.write(std::as_bytes(std::span<const Index>(_values->values)));
    }
    template<index_element Index, MemoryProperty property>
    template<buffer_storage_policy Policy>
    TypedIndexBuffer<Index, property>::TypedIndexBuffer(LogicalDeviceAccessor device, CommandManager manager, std::span<const Index> values, Policy policy)
    requires(!contain<property, MemoryProperty::HOST_VISIBLE>())
        : _values(MakeSnapshot(values)), _buffer(detail::make_buffer_storage(device, std::move(manager), {helper::checked_buffer_array_size<Index>(_values->values.size()), USAGE, property}, std::as_bytes(std::span<const Index>(_values->values)), policy)) {}

    template<index_element Index, MemoryProperty property>
    void TypedIndexBuffer<Index, property>::copy(std::span<const Index> values)
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
    {
        const auto buffer = _buffer;
        const auto state = _values;
        buffer.ref().use().require_idle();
        if (values.size() != state->values.size())
            throw std::invalid_argument("Typed index update must preserve the element count");
        if (state->revision == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Typed index revision exhausted");
        std::vector<Index> snapshot(values.begin(), values.end());
        // 書込み中の失敗でGPU内容が不明になった場合も、古い検証結果は利用しない。
        ++state->revision;
        state->valid = false;
        buffer.write(std::as_bytes(std::span<const Index>(snapshot)));
        state->values.swap(snapshot);
        state->valid = true;
    }
    template<index_element Index, MemoryProperty property>
    std::pair<std::uint32_t, std::uint32_t> TypedIndexBuffer<Index, property>::bounds(size_t first, size_t count) const {
        if (!_values->valid)
            throw std::logic_error("Typed index data requires a successful upload");
        if (first > size() || count > size() - first)
            throw std::out_of_range("Typed indexed draw exceeds its index buffer");
        if (!count)
            return {0, 0};
        const auto values = std::span<const Index>(_values->values).subspan(first, count);
        const auto [low, high] = std::ranges::minmax_element(values);
        return {*low, *high};
    }
} // namespace VSL_NAMESPACE::typed
