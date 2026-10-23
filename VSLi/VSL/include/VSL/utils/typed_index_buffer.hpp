#pragma once
#include "../vulkan/buffer.hpp"
#include "../vulkan/commands/bind_index_buffers.hpp"
#include <algorithm>
#include <concepts>
#include <span>
#include <stdexcept>

namespace VSL_NAMESPACE::typed {
    template<typename T>
    concept index_element = std::same_as<T, std::uint16_t> || std::same_as<T, std::uint32_t>;

    template<index_element Index_, MemoryProperty property = MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT>
    class TypedIndexBuffer {
    public:
        using Index = Index_;
        static constexpr command::IndexType INDEX_TYPE = sizeof(Index) == 2 ? command::IndexType::UINT16 : command::IndexType::UINT32;

    private:
        using Storage = Buffer<MemoryType::INDEX_BUFFER | MemoryType::TRANSFER_DESTINATION, property>;
        LogicalDeviceAccessor _device;
        Storage _buffer;
        std::shared_ptr<std::vector<Index>> _values;

    public:
        TypedIndexBuffer(LogicalDeviceAccessor device, std::span<const Index> values)
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        TypedIndexBuffer(LogicalDeviceAccessor device, CommandManager manager, std::span<const Index> values)
        requires(!contain<property, MemoryProperty::HOST_VISIBLE>())
            : _device(device), _buffer(device, manager, values.data(), values.size()), _values(std::make_shared<std::vector<Index>>(values.begin(), values.end())) {}
        size_t size() const { return _values->size(); }
        LogicalDeviceAccessor device() const { return _device; }
        const Storage& backend() const { return _buffer; }
        Storage& backend() { return _buffer; }
        void copy(std::span<const Index> values)
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        std::pair<std::uint32_t, std::uint32_t> bounds(size_t first, size_t count) const;
    };

    template<index_element Index, MemoryProperty property>
    TypedIndexBuffer<Index, property>::TypedIndexBuffer(LogicalDeviceAccessor device, std::span<const Index> values)
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
        : _device(device), _buffer(device, helper::checked_buffer_array_size<Index>(values.size())), _values(std::make_shared<std::vector<Index>>(values.begin(), values.end())) {
        if (!_buffer.copy(values.data(), values.size()))
            throw std::runtime_error("Typed index upload failed");
    }
    template<index_element Index, MemoryProperty property>
    void TypedIndexBuffer<Index, property>::copy(std::span<const Index> values)
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
    {
        if (values.size() != size())
            throw std::invalid_argument("Typed index update must preserve the element count");
        std::vector<Index> snapshot(values.begin(), values.end());
        if (!_buffer.copy(snapshot.data(), snapshot.size()))
            throw std::runtime_error("Typed index upload failed");
        _values->swap(snapshot);
    }
    template<index_element Index, MemoryProperty property>
    std::pair<std::uint32_t, std::uint32_t> TypedIndexBuffer<Index, property>::bounds(size_t first, size_t count) const {
        if (first > size() || count > size() - first)
            throw std::out_of_range("Typed indexed draw exceeds its index buffer");
        if (!count)
            return {0, 0};
        const auto values = std::span<const Index>(*_values).subspan(first, count);
        const auto [low, high] = std::ranges::minmax_element(values);
        return {*low, *high};
    }
} // namespace VSL_NAMESPACE::typed
