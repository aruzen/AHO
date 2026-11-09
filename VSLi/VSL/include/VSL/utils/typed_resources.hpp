#pragma once
#include "typed_descriptor_schema.hpp"
#include "typed_buffer_view.hpp"
#include "../vulkan/descriptor_set_manager.hpp"
#include "../vulkan/resource_use.hpp"
#include <algorithm>
#include <concepts>
#include <cstring>
#include <span>
#include <stdexcept>

namespace VSL_NAMESPACE::typed {
    namespace detail {
        class DescriptorValue {
            lifetime::OwnedRef<impl::BufferImpl> _buffer;
            lifetime::OwnedRef<impl::ImageImpl> _image;
            lifetime::OwnedRef<impl::SamplerImpl> _sampler;
            size_t _bytes = 0;
            size_t _offset = 0;
            ResourceUse _use;
            friend ResourceUse descriptor_constraints(const DescriptorValue& value);
            friend void write_descriptor(GraphicResource resource, graphic_resource::BindingPoint point, std::span<const DescriptorValue> values);

        public:
            DescriptorValue() = default;
            static DescriptorValue FromBuffer(BufferView source, ResourceUse use = {});
            static DescriptorValue FromImage(ImageAccessor source, ResourceUse use = {});
            static DescriptorValue FromSampler(SamplerAccessor source, ResourceUse use = {});
            static DescriptorValue FromTexture(ImageAccessor image, SamplerAccessor sampler, ResourceUse use = {});
            size_t byte_offset() const { return _offset; }
            size_t size() const { return _bytes; }
            DescriptorValue buffer_subview(size_t offset, size_t bytes) const;
            ImageAccessor image() const;
            SamplerAccessor sampler() const;
            ResourceUse resource_use() const { return _use; }
        };
        ResourceUse descriptor_constraints(const DescriptorValue& value);
        void require_descriptor_complete(const GraphicResource& resource);
        void freeze_descriptor_dependencies(const GraphicResource& resource);
        void validate_descriptor_resource(const GraphicResource& resource, LogicalDeviceAccessor device, std::span<const graphic_resource::BindingPoint> points);
        GraphicResource allocate_descriptor_set(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager,
                                                std::span<const graphic_resource::BindingPoint> points);
        void write_descriptor(GraphicResource resource, graphic_resource::BindingPoint point, std::span<const DescriptorValue> values);
        template<typename Binding, typename Value>
        consteval bool descriptor_matches();
    } // namespace detail

    template<typename Payload_, graphic_resource::Type kind, MemoryProperty property>
    class TypedResourceBuffer;
    template<typename Payload_, graphic_resource::Type kind>
    class TypedResourceBufferView {
        template<typename, graphic_resource::Type, MemoryProperty>
        friend class TypedResourceBuffer;
        detail::DescriptorValue _value;
        size_t _count;
        TypedResourceBufferView(detail::DescriptorValue value, size_t count) : _value(std::move(value)), _count(count) {}

    public:
        using Payload = typename PayloadLayout<Payload_>::Payload;
        static constexpr auto KIND = kind;
        static_assert(kind == graphic_resource::Type::UNIFORM_BUFFER || kind == graphic_resource::Type::STORAGE_BUFFER);
        static_assert(std::is_standard_layout_v<Payload> && std::is_trivially_copyable_v<Payload>);
        static TypedResourceBufferView FromBuffer(const BufferAccessor& source, size_t count, size_t byte_offset = 0, ResourceUse use = {});
        static TypedResourceBufferView FromView(const BufferView& source, size_t count, size_t byte_offset = 0, ResourceUse use = {});
        size_t size() const { return _count; }
        size_t byte_offset() const { return _value.byte_offset(); }
        detail::DescriptorValue descriptor() const { return _value; }
        TypedResourceBufferView view(size_t first, size_t count) const;
    };

    template<typename Payload_, graphic_resource::Type kind, MemoryProperty property = MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT>
    class TypedResourceBuffer {
    public:
        using Payload = typename PayloadLayout<Payload_>::Payload;

    private:
        static_assert(kind == graphic_resource::Type::UNIFORM_BUFFER || kind == graphic_resource::Type::STORAGE_BUFFER);
        static_assert(std::is_trivially_copyable_v<Payload> && std::is_standard_layout_v<Payload>);
        static constexpr MemoryType USAGE = kind == graphic_resource::Type::UNIFORM_BUFFER ? MemoryType::UNIFORM_BUFFER : MemoryType::STORAGE_BUFFER;
        BufferView _buffer;
        size_t _count;

    public:
        static constexpr auto KIND = kind;
        template<buffer_storage_policy Policy = DefaultStorage>
        TypedResourceBuffer(LogicalDeviceAccessor device, std::in_place_t, size_t count, Policy policy = {})
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        template<buffer_storage_policy Policy = DefaultStorage>
        TypedResourceBuffer(LogicalDeviceAccessor device, std::span<const Payload> values, Policy policy = {})
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        template<buffer_storage_policy Policy = DefaultStorage>
        TypedResourceBuffer(LogicalDeviceAccessor device, const Payload& value, Policy policy = {})
        requires(contain<property, MemoryProperty::HOST_VISIBLE>())
            : TypedResourceBuffer(device, std::span<const Payload>(&value, 1), policy) {}
        template<buffer_storage_policy Policy = DefaultStorage>
        TypedResourceBuffer(LogicalDeviceAccessor device, CommandManager manager, std::span<const Payload> values, Policy policy = {})
        requires(!contain<property, MemoryProperty::HOST_VISIBLE>());
        const BufferView& backend() const& { return _buffer; }
        const BufferView& backend() const&& = delete;
        BufferView storage_view() const { return _buffer; }
        size_t size() const { return _count; }
        TypedResourceBufferView<Payload_, kind> view(size_t first, size_t count) const;
        void copy(std::span<const Payload> values)
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        void read_into(std::span<Payload> destination, size_t first = 0) const
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        detail::DescriptorValue descriptor() const;
    };
    template<typename Payload, MemoryProperty property = MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT>
    using TypedUniformBuffer = TypedResourceBuffer<Payload, graphic_resource::Type::UNIFORM_BUFFER, property>;
    template<typename Payload, MemoryProperty property = MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT>
    using TypedStorageBuffer = TypedResourceBuffer<Payload, graphic_resource::Type::STORAGE_BUFFER, property>;

    template<graphic_resource::Type kind>
    class TypedImageResource {
        detail::DescriptorValue _value;

    public:
        using Payload = void;
        static constexpr auto KIND = kind;
        TypedImageResource(ImageAccessor image, SamplerAccessor sampler, ResourceUse use = {})
        requires(kind == graphic_resource::Type::COMBINED_IMAGE_SAMPLER)
            : _value(detail::DescriptorValue::FromTexture(std::move(image), std::move(sampler), std::move(use))) {}
        explicit TypedImageResource(ImageAccessor image, ResourceUse use = {})
        requires(kind == graphic_resource::Type::SAMPLED_IMAGE || kind == graphic_resource::Type::STORAGE_IMAGE)
            : _value(detail::DescriptorValue::FromImage(std::move(image), std::move(use))) {}
        explicit TypedImageResource(SamplerAccessor sampler, ResourceUse use = {})
        requires(kind == graphic_resource::Type::SAMPLER)
            : _value(detail::DescriptorValue::FromSampler(std::move(sampler), std::move(use))) {}
        detail::DescriptorValue descriptor() const { return _value; }
    };
    using TypedTexture = TypedImageResource<graphic_resource::Type::COMBINED_IMAGE_SAMPLER>;
    using TypedSampledImage = TypedImageResource<graphic_resource::Type::SAMPLED_IMAGE>;
    using TypedStorageImage = TypedImageResource<graphic_resource::Type::STORAGE_IMAGE>;
    using TypedSampler = TypedImageResource<graphic_resource::Type::SAMPLER>;

    template<typename Spec_>
    class TypedDescriptorSet {
        GraphicResource _resource;
        LogicalDeviceAccessor _device;
        std::shared_ptr<ResourceUseGroup> _use;
        TypedDescriptorSet(LogicalDeviceAccessor device, GraphicResource resource, ResourceRetention retention);

    public:
        using Spec = Spec_;
        using Layout = typename Spec::Layout;
        TypedDescriptorSet(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, ResourceRetention retention = ResourceRetention::COMPLETION);
        static TypedDescriptorSet FromResource(LogicalDeviceAccessor device, GraphicResource resource, ResourceRetention retention = ResourceRetention::COMPLETION);
        const GraphicResource& backend() const { return _resource; }
        LogicalDeviceAccessor device() const { return _device; }
        const std::shared_ptr<ResourceUseGroup>& resource_use() const { return _use; }
        void require_complete() const;
        void freeze_dependencies() const { detail::freeze_descriptor_dependencies(_resource); }
        template<std::uint32_t binding, typename Value>
        requires(detail::descriptor_matches<typename Spec_::template Binding<binding>, Value>() && Spec_::template Binding<binding>::POINT.amount == 1)
        void write(const Value& value);
        template<std::uint32_t binding, typename Value, size_t count>
        requires(detail::descriptor_matches<typename Spec_::template Binding<binding>, Value>() && Spec_::template Binding<binding>::POINT.amount == count)
        void write(const std::array<Value, count>& values);
    };

    namespace detail {
        template<typename T>
        struct IsDescriptorSet : std::false_type {};
        template<typename Spec>
        struct IsDescriptorSet<TypedDescriptorSet<Spec>> : std::true_type {};
        template<typename Schema, typename... Sets, size_t... indices>
        consteval bool descriptor_sets_match(std::index_sequence<indices...>);
        template<typename Binding, typename Value>
        consteval bool descriptor_matches() {
            if constexpr (std::is_void_v<Binding> || !requires { typename Value::Payload; Value::KIND; })
                return false;
            else
                return std::same_as<typename Binding::Payload, typename Value::Payload> && Binding::POINT.binding_type == Value::KIND;
        }
        template<typename Schema, typename... Sets, size_t... indices>
        consteval bool descriptor_sets_match(std::index_sequence<indices...>) {
            if constexpr (sizeof...(Sets) != Schema::COUNT || !(IsDescriptorSet<std::remove_cvref_t<Sets>>::value && ...))
                return false;
            else
                return (std::same_as<typename std::remove_cvref_t<Sets>::Layout, typename Schema::template Set<indices>::Layout> && ...);
        }
    } // namespace detail
    template<typename Schema, typename... Sets>
    concept compatible_descriptor_sets = detail::descriptor_sets_match<Schema, Sets...>(std::index_sequence_for<Sets...>{});

    template<typename Payload_, graphic_resource::Type kind>
    TypedResourceBufferView<Payload_, kind> TypedResourceBufferView<Payload_, kind>::FromBuffer(const BufferAccessor& source, size_t count, size_t byte_offset, ResourceUse use) {
        return FromView(BufferView::FromBuffer(source), count, byte_offset, std::move(use));
    }
    template<typename Payload_, graphic_resource::Type kind>
    TypedResourceBufferView<Payload_, kind> TypedResourceBufferView<Payload_, kind>::FromView(const BufferView& source, size_t count, size_t byte_offset, ResourceUse use) {
        constexpr auto usage = kind == graphic_resource::Type::UNIFORM_BUFFER ? MemoryType::UNIFORM_BUFFER : MemoryType::STORAGE_BUFFER;
        detail::validate_buffer_view(source, usage, byte_offset, count, sizeof(Payload), alignof(Payload));
        return TypedResourceBufferView(detail::DescriptorValue::FromBuffer(source.view(byte_offset, count * sizeof(Payload)), std::move(use)), count);
    }
    template<typename Payload_, graphic_resource::Type kind>
    TypedResourceBufferView<Payload_, kind> TypedResourceBufferView<Payload_, kind>::view(size_t first, size_t count) const {
        const auto offset = detail::checked_view_offset(0, first, count, _count, sizeof(Payload));
        return TypedResourceBufferView(_value.buffer_subview(offset, count * sizeof(Payload)), count);
    }
    template<typename Payload_, graphic_resource::Type kind, MemoryProperty property>
    detail::DescriptorValue TypedResourceBuffer<Payload_, kind, property>::descriptor() const {
        return TypedResourceBufferView<Payload_, kind>::FromView(_buffer, _count).descriptor();
    }
    template<typename Payload_, graphic_resource::Type kind, MemoryProperty property>
    TypedResourceBufferView<Payload_, kind> TypedResourceBuffer<Payload_, kind, property>::view(size_t first, size_t count) const {
        const auto offset = detail::checked_view_offset(0, first, count, _count, sizeof(Payload));
        return TypedResourceBufferView<Payload_, kind>(descriptor().buffer_subview(offset, count * sizeof(Payload)), count);
    }
    template<typename PayloadTag, graphic_resource::Type kind, MemoryProperty property>
    template<buffer_storage_policy Policy>
    TypedResourceBuffer<PayloadTag, kind, property>::TypedResourceBuffer(LogicalDeviceAccessor device, std::in_place_t, size_t count, Policy policy)
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
        : _buffer(detail::make_buffer_storage(device, {helper::checked_buffer_array_size<Payload>(count), USAGE | MemoryType::TRANSFER_DESTINATION, property}, policy)), _count(count) {
        _buffer.ref().fill(std::byte{0});
    }
    template<typename PayloadTag, graphic_resource::Type kind, MemoryProperty property>
    template<buffer_storage_policy Policy>
    TypedResourceBuffer<PayloadTag, kind, property>::TypedResourceBuffer(LogicalDeviceAccessor device, std::span<const Payload> values, Policy policy)
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
        : _buffer(detail::make_buffer_storage(device, {helper::checked_buffer_array_size<Payload>(values.size()), USAGE | MemoryType::TRANSFER_DESTINATION, property}, policy)), _count(values.size()) {
        copy(values);
    }
    template<typename PayloadTag, graphic_resource::Type kind, MemoryProperty property>
    template<buffer_storage_policy Policy>
    TypedResourceBuffer<PayloadTag, kind, property>::TypedResourceBuffer(LogicalDeviceAccessor device, CommandManager manager, std::span<const Payload> values, Policy policy)
    requires(!contain<property, MemoryProperty::HOST_VISIBLE>())
        : _buffer(detail::make_buffer_storage(device, std::move(manager), {helper::checked_buffer_array_size<Payload>(values.size()), USAGE | MemoryType::TRANSFER_DESTINATION, property}, std::as_bytes(values), policy)), _count(values.size()) {}

    template<typename PayloadTag, graphic_resource::Type kind, MemoryProperty property>
    void TypedResourceBuffer<PayloadTag, kind, property>::copy(std::span<const Payload> values)
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
    {
        const auto buffer = _buffer;
        const auto count = _count;
        buffer.ref().use().require_idle();
        if (values.size() != count)
            throw std::invalid_argument("Typed descriptor buffer update must preserve record count");
        buffer.write(std::as_bytes(values));
    }
    template<typename PayloadTag, graphic_resource::Type kind, MemoryProperty property>
    void TypedResourceBuffer<PayloadTag, kind, property>::read_into(std::span<Payload> destination, size_t first) const
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
    {
        const auto buffer = _buffer;
        const auto count = _count;
        if (first > count || destination.size() > count - first)
            throw std::out_of_range("Typed descriptor read exceeds its buffer");
        buffer.read_into(std::as_writable_bytes(destination), first * sizeof(Payload));
    }
    template<typename Spec>
    TypedDescriptorSet<Spec>::TypedDescriptorSet(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, ResourceRetention retention)
        : TypedDescriptorSet(device, detail::allocate_descriptor_set(device, manager, Spec::POINTS), retention) {}
    template<typename Spec>
    TypedDescriptorSet<Spec>::TypedDescriptorSet(LogicalDeviceAccessor device, GraphicResource resource, ResourceRetention retention)
        : _resource(std::move(resource)), _device(device) {
        detail::validate_descriptor_resource(_resource, device, Spec::POINTS);
        _use = ResourceUseGroup::ForDescriptor(Spec::BINDING_COUNT, _resource, retention);
    }
    template<typename Spec>
    TypedDescriptorSet<Spec> TypedDescriptorSet<Spec>::FromResource(LogicalDeviceAccessor device, GraphicResource resource, ResourceRetention retention) {
        return TypedDescriptorSet(device, std::move(resource), retention);
    }
    template<typename Spec>
    void TypedDescriptorSet<Spec>::require_complete() const {
        detail::require_descriptor_complete(_resource);
    }
    template<typename Spec_>
    template<std::uint32_t binding, typename Value>
    requires(detail::descriptor_matches<typename Spec_::template Binding<binding>, Value>() && Spec_::template Binding<binding>::POINT.amount == 1)
    void TypedDescriptorSet<Spec_>::write(const Value& value) {
        const auto descriptor = value.descriptor();
        detail::write_descriptor(_resource, Spec::template Binding<binding>::POINT, std::span(&descriptor, 1));
    }
    template<typename Spec_>
    template<std::uint32_t binding, typename Value, size_t count>
    requires(detail::descriptor_matches<typename Spec_::template Binding<binding>, Value>() && Spec_::template Binding<binding>::POINT.amount == count)
    void TypedDescriptorSet<Spec_>::write(const std::array<Value, count>& values) {
        std::array<detail::DescriptorValue, count> descriptors{};
        for (size_t i = 0; i < count; ++i)
            descriptors[i] = values[i].descriptor();
        detail::write_descriptor(_resource, Spec::template Binding<binding>::POINT, descriptors);
    }
} // namespace VSL_NAMESPACE::typed
