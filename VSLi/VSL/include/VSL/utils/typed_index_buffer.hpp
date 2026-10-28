#pragma once
#include "../vulkan/buffer.hpp"
#include "../vulkan/resource_use.hpp"
#include "../vulkan/commands/bind_index_buffers.hpp"
#include "typed_buffer_view.hpp"
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
            std::pair<std::uint32_t, std::uint32_t> bounds(size_t first, size_t count) const;
        };
    } // namespace detail

    template<index_element Index_, MemoryProperty property>
    class TypedIndexBuffer;

    template<index_element Index_>
    class TypedIndexBufferView {
        template<index_element, MemoryProperty>
        friend class TypedIndexBuffer;
        template<typename, typename, typename, bool, bool>
        friend class TypedVertexBinding;
        LogicalDeviceAccessor _device;
        std::shared_ptr<impl::BufferImpl> _buffer;
        std::shared_ptr<detail::IndexSnapshot<Index_>> _values;
        ResourceUse _use;
        size_t _first, _count;
        TypedIndexBufferView(LogicalDeviceAccessor device, std::shared_ptr<impl::BufferImpl> buffer,
                             std::shared_ptr<detail::IndexSnapshot<Index_>> values, ResourceUse use, size_t first, size_t count)
            : _device(device), _buffer(std::move(buffer)), _values(std::move(values)), _use(std::move(use)), _first(first), _count(count) {}

    public:
        using Index = Index_;
        static constexpr command::IndexType INDEX_TYPE = sizeof(Index) == 2 ? command::IndexType::UINT16 : command::IndexType::UINT32;
        size_t size() const { return _count; }
        size_t byte_offset() const { return _first * sizeof(Index); }
        LogicalDeviceAccessor device() const { return _device; }
        const ResourceUse& resource_use() const { return _use; }
        const std::shared_ptr<impl::BufferImpl>& backend() const { return _buffer; }
        TypedIndexBufferView view(size_t first, size_t count) const;
        std::pair<std::uint32_t, std::uint32_t> bounds(size_t first, size_t count) const;
    };

    template<index_element Index_, MemoryProperty property = MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT>
    class TypedIndexBuffer {
    public:
        using Index = Index_;
        static constexpr command::IndexType INDEX_TYPE = sizeof(Index) == 2 ? command::IndexType::UINT16 : command::IndexType::UINT32;

    private:
        template<typename, typename, typename, bool, bool>
        friend class TypedVertexBinding;
        using Storage = Buffer<MemoryType::INDEX_BUFFER | MemoryType::TRANSFER_DESTINATION, property>;
        LogicalDeviceAccessor _device;
        Storage _buffer;
        std::shared_ptr<detail::IndexSnapshot<Index>> _values;
        std::shared_ptr<ResourceUseState> _use = std::make_shared<ResourceUseState>();

    public:
        TypedIndexBuffer(LogicalDeviceAccessor device, std::span<const Index> values)
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        TypedIndexBuffer(LogicalDeviceAccessor device, CommandManager manager, std::span<const Index> values)
        requires(!contain<property, MemoryProperty::HOST_VISIBLE>())
            : _device(device), _buffer(device, manager, values.data(), values.size()), _values(std::make_shared<detail::IndexSnapshot<Index>>(values)) {}
        size_t size() const { return _values->values.size(); }
        size_t byte_offset() const { return 0; }
        LogicalDeviceAccessor device() const { return _device; }
        ResourceUse resource_use() const { return {_use, _buffer.data_}; }
        const Storage& backend() const { return _buffer; }
        Storage& backend() { return _buffer; }
        void copy(std::span<const Index> values)
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        std::pair<std::uint32_t, std::uint32_t> bounds(size_t first, size_t count) const;
        TypedIndexBufferView<Index> view(size_t first, size_t count) const;
    };

    namespace detail {
        template<index_element Index>
        std::pair<std::uint32_t, std::uint32_t> IndexSnapshot<Index>::bounds(size_t first, size_t count) const {
            if (!valid)
                throw std::logic_error("Typed index data requires a successful upload");
            if (first > values.size() || count > values.size() - first)
                throw std::out_of_range("Typed indexed draw exceeds its index buffer");
            if (!count)
                return {0, 0};
            const auto range = std::span<const Index>(values).subspan(first, count);
            const auto [low, high] = std::ranges::minmax_element(range);
            return {*low, *high};
        }
    } // namespace detail

    template<index_element Index>
    TypedIndexBufferView<Index> TypedIndexBufferView<Index>::view(size_t first, size_t count) const {
        const auto offset = detail::checked_view_offset(byte_offset(), first, count, size(), sizeof(Index));
        return TypedIndexBufferView(_device, _buffer, _values, _use, offset / sizeof(Index), count);
    }

    template<index_element Index>
    std::pair<std::uint32_t, std::uint32_t> TypedIndexBufferView<Index>::bounds(size_t first, size_t count) const {
        if (first > size() || count > size() - first)
            throw std::out_of_range("Typed indexed draw exceeds its index view");
        return _values->bounds(_first + first, count);
    }

    template<index_element Index, MemoryProperty property>
    TypedIndexBuffer<Index, property>::TypedIndexBuffer(LogicalDeviceAccessor device, std::span<const Index> values)
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
        : _device(device), _buffer(device, helper::checked_buffer_array_size<Index>(values.size())), _values(std::make_shared<detail::IndexSnapshot<Index>>(values)) {
        if (!_buffer.copy(values.data(), values.size()))
            throw std::runtime_error("Typed index upload failed");
    }
    template<index_element Index, MemoryProperty property>
    void TypedIndexBuffer<Index, property>::copy(std::span<const Index> values)
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
    {
        _use->require_idle();
        if (values.size() != size())
            throw std::invalid_argument("Typed index update must preserve the element count");
        if (_values->revision == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Typed index revision exhausted");
        std::vector<Index> snapshot(values.begin(), values.end());
        // 書込み中の失敗でGPU内容が不明になった場合も、古い検証結果は利用しない。
        ++_values->revision;
        _values->valid = false;
        if (!_buffer.copy(snapshot.data(), snapshot.size()))
            throw std::runtime_error("Typed index upload failed");
        _values->values.swap(snapshot);
        _values->valid = true;
    }
    template<index_element Index, MemoryProperty property>
    std::pair<std::uint32_t, std::uint32_t> TypedIndexBuffer<Index, property>::bounds(size_t first, size_t count) const {
        return _values->bounds(first, count);
    }

    template<index_element Index, MemoryProperty property>
    TypedIndexBufferView<Index> TypedIndexBuffer<Index, property>::view(size_t first, size_t count) const {
        const auto offset = detail::checked_view_offset(0, first, count, size(), sizeof(Index));
        return TypedIndexBufferView<Index>(_device, _buffer.data_, _values, resource_use(), offset / sizeof(Index), count);
    }
} // namespace VSL_NAMESPACE::typed
