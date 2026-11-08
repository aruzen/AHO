#pragma once
#include "../utils/typed_descriptor_schema.hpp"
#include <optional>

namespace VSL_NAMESPACE::typed {
    namespace detail {
        template<typename Resources>
        struct DescriptorLayoutShape {
            static constexpr size_t SetIndex(std::uint32_t set);
            static constexpr bool HAS_HOLES = Resources::SET_COUNT > Resources::COUNT;
            static constexpr size_t EMPTY_INDEX = []<size_t... indices>(std::index_sequence<indices...>) {
                const std::array<bool, Resources::COUNT> empty{Resources::template Set<indices>::POINTS.empty()...};
                for (size_t i = 0; i < empty.size(); ++i)
                    if (empty[i])
                        return i;
                return empty.size();
            }(std::make_index_sequence<Resources::COUNT>{});
            static constexpr bool NEEDS_EMPTY = HAS_HOLES && EMPTY_INDEX == Resources::COUNT;
        };
        LogicalDeviceAccessor descriptor_layout_device(std::span<const graphic_resource::BindingLayout> layouts);
        void validate_descriptor_layout_source(LogicalDeviceAccessor device, const graphic_resource::BindingLayout& layout,
                                               std::span<const graphic_resource::BindingPoint> points);
        void inject_descriptor_layouts(PipelineLayoutEditAccessor layout, LogicalDeviceAccessor device,
                                       std::span<const graphic_resource::BindingLayout> sources, std::span<const std::uint32_t> numbers,
                                       const std::optional<graphic_resource::BindingLayout>& empty, std::uint32_t set_count);
        template<typename Resources>
        struct DescriptorLayoutInjection;
    } // namespace detail

    // owning collection。schemaだけでなく実際のsource構造を検査し、bundle/frame facadeは保持しない。
    template<typename Resources>
    class TypedDescriptorLayouts {
        using Shape = detail::DescriptorLayoutShape<Resources>;
        LogicalDeviceAccessor _device;
        std::array<graphic_resource::BindingLayout, Resources::COUNT> _layouts;
        std::optional<graphic_resource::BindingLayout> _empty;
        friend struct detail::DescriptorLayoutInjection<Resources>;
        void inject(PipelineLayoutEditAccessor layout) const;
        void swap_state(TypedDescriptorLayouts& other) noexcept;

    public:
        using ResourceLayout = Resources;
        explicit TypedDescriptorLayouts(std::array<graphic_resource::BindingLayout, Resources::COUNT> layouts,
                                        std::optional<graphic_resource::BindingLayout> empty = std::nullopt);
        TypedDescriptorLayouts(const TypedDescriptorLayouts&) = default;
        TypedDescriptorLayouts(TypedDescriptorLayouts&&) noexcept = default;
        TypedDescriptorLayouts& operator=(TypedDescriptorLayouts other) noexcept;
        template<std::uint32_t set>
        requires(detail::DescriptorLayoutShape<Resources>::SetIndex(set) < Resources::COUNT)
        graphic_resource::BindingLayout layout() const;
        std::optional<graphic_resource::BindingLayout> hole_layout() const;
    };
    namespace detail {
        template<typename Resources>
        struct DescriptorLayoutInjection {
            const TypedDescriptorLayouts<Resources>& source;
            void injection(PipelineLayoutEditAccessor layout) const { source.inject(std::move(layout)); }
        };
    } // namespace detail

    template<typename Resources>
    constexpr size_t detail::DescriptorLayoutShape<Resources>::SetIndex(std::uint32_t set) {
        for (size_t i = 0; i < Resources::COUNT; ++i)
            if (Resources::SET_NUMBERS[i] == set)
                return i;
        return Resources::COUNT;
    }
    template<typename Resources>
    TypedDescriptorLayouts<Resources>::TypedDescriptorLayouts(std::array<graphic_resource::BindingLayout, Resources::COUNT> layouts,
                                                              std::optional<graphic_resource::BindingLayout> empty)
        : _device(detail::descriptor_layout_device(layouts)), _layouts(std::move(layouts)), _empty(std::move(empty)) {
        [&]<size_t... indices>(std::index_sequence<indices...>) {
            (detail::validate_descriptor_layout_source(_device, _layouts[indices], Resources::template Set<indices>::POINTS), ...);
        }(std::make_index_sequence<Resources::COUNT>{});
        if constexpr (Shape::NEEDS_EMPTY) {
            if (!_empty)
                throw std::invalid_argument("Sparse typed layouts require an empty layout source");
            detail::validate_descriptor_layout_source(_device, *_empty, {});
        } else if (_empty) {
            throw std::invalid_argument("Typed layouts do not require an additional empty source");
        }
    }
    template<typename Resources>
    TypedDescriptorLayouts<Resources>& TypedDescriptorLayouts<Resources>::operator=(TypedDescriptorLayouts other) noexcept {
        swap_state(other);
        return *this;
    }
    template<typename Resources>
    void TypedDescriptorLayouts<Resources>::swap_state(TypedDescriptorLayouts& other) noexcept {
        _device.data_.swap(other._device.data_);
        _layouts.swap(other._layouts);
        _empty.swap(other._empty);
    }
    template<typename Resources>
    template<std::uint32_t set>
    requires(detail::DescriptorLayoutShape<Resources>::SetIndex(set) < Resources::COUNT)
    graphic_resource::BindingLayout TypedDescriptorLayouts<Resources>::layout() const {
        constexpr auto index = detail::DescriptorLayoutShape<Resources>::SetIndex(set);
        detail::validate_descriptor_layout_source(_device, _layouts[index], Resources::template Set<index>::POINTS);
        return _layouts[index];
    }
    template<typename Resources>
    std::optional<graphic_resource::BindingLayout> TypedDescriptorLayouts<Resources>::hole_layout() const {
        if constexpr (Shape::HAS_HOLES) {
            if constexpr (Shape::NEEDS_EMPTY) {
                if (!_empty)
                    throw std::invalid_argument("Typed layout collection has been moved");
                detail::validate_descriptor_layout_source(_device, *_empty, {});
                return _empty;
            } else {
                const auto& value = _layouts[Shape::EMPTY_INDEX];
                detail::validate_descriptor_layout_source(_device, value, {});
                return value;
            }
        }
        return std::nullopt;
    }
    template<typename Resources>
    void TypedDescriptorLayouts<Resources>::inject(PipelineLayoutEditAccessor layout) const {
        detail::inject_descriptor_layouts(std::move(layout), _device, _layouts, Resources::SET_NUMBERS, hole_layout(), Resources::SET_COUNT);
    }
} // namespace VSL_NAMESPACE::typed
