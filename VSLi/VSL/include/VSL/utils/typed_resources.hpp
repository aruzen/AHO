#pragma once
#include "typed_block_layout.hpp"
#include "typed_buffer_view.hpp"
#include "../vulkan/descriptor_set_manager.hpp"
#include "../vulkan/stages/resource_binding.hpp"
#include <algorithm>
#include <concepts>
#include <span>
#include <stdexcept>

namespace VSL_NAMESPACE::typed {
    namespace detail {
        struct DescriptorValue {
            std::shared_ptr<impl::BufferImpl> buffer;
            std::shared_ptr<impl::ImageImpl> image;
            std::shared_ptr<impl::SamplerImpl> sampler;
            size_t bytes = 0;
            size_t offset = 0;
        };
        struct DescriptorRequirement {
            std::uint32_t set;
            graphic_resource::BindingPoint point;
            const BlockType* memory_layout = nullptr;
            size_t payload_size = 0;
        };
        void write_descriptor(GraphicResource resource, graphic_resource::BindingPoint point, std::span<const DescriptorValue> values);
        void inject_resource_schema(PipelineLayoutEditAccessor layout, std::span<const DescriptorRequirement> requirements, std::uint32_t set_count);
        void validate_resource_schema(PipelineLayoutAccessor layout, std::span<const DescriptorRequirement> requirements);
        template<std::uint32_t binding, typename... Bindings>
        struct FindDescriptor;
        template<std::uint32_t binding>
        struct FindDescriptor<binding> {
            using Type = void;
        };
        template<std::uint32_t binding, typename First, typename... Rest>
        struct FindDescriptor<binding, First, Rest...> {
            using Type = std::conditional_t<First::POINT.binding == binding, First, typename FindDescriptor<binding, Rest...>::Type>;
        };
        template<typename Binding, typename Value>
        consteval bool descriptor_matches();
    } // namespace detail

    template<std::uint32_t binding, graphic_resource::Type kind, ShaderFlag stages, typename Payload_ = void, std::uint32_t count = 1>
    struct DescriptorBinding {
        using MemoryLayout = PayloadLayout<Payload_>;
        using Payload = typename MemoryLayout::Payload;
        static constexpr const BlockType* MEMORY_LAYOUT = std::is_void_v<Payload> ? nullptr : &MemoryLayout::LAYOUT;
        static constexpr size_t PAYLOAD_SIZE = [] { if constexpr (std::is_void_v<Payload>) return size_t{0}; else return sizeof(Payload); }();
        static_assert(std::is_void_v<Payload> || detail::block_layout_known(MemoryLayout::LAYOUT), "Typed descriptor payload requires BlockRecord/BlockLayout metadata");
        static_assert(MemoryLayout::LAYOUT.size <= PAYLOAD_SIZE, "Typed descriptor layout exceeds payload sizeof");
        static constexpr graphic_resource::BindingPoint POINT{binding, kind, stages, count};
        static_assert(std::is_void_v<Payload> || (std::is_trivially_copyable_v<Payload> && std::is_standard_layout_v<Payload>), "Typed descriptor payload must be a trivial standard-layout record");
        static_assert(binding != UINT32_MAX && count != 0 && stages != ShaderFlag::ERROR_, "Invalid typed descriptor binding/count/stages");
        static_assert(kind == graphic_resource::Type::UNIFORM_BUFFER || kind == graphic_resource::Type::STORAGE_BUFFER ||
                          kind == graphic_resource::Type::COMBINED_IMAGE_SAMPLER || kind == graphic_resource::Type::SAMPLED_IMAGE ||
                          kind == graphic_resource::Type::STORAGE_IMAGE || kind == graphic_resource::Type::SAMPLER,
                      "Unsupported typed descriptor kind");
        static_assert((kind == graphic_resource::Type::UNIFORM_BUFFER || kind == graphic_resource::Type::STORAGE_BUFFER) != std::is_void_v<Payload>,
                      "Typed buffer descriptors require a payload; image descriptors use void");
    };
    template<std::uint32_t binding, typename Payload, ShaderFlag stages, std::uint32_t count = 1>
    using UniformBinding = DescriptorBinding<binding, graphic_resource::Type::UNIFORM_BUFFER, stages, Payload, count>;
    template<std::uint32_t binding, typename Payload, ShaderFlag stages, std::uint32_t count = 1>
    using StorageBinding = DescriptorBinding<binding, graphic_resource::Type::STORAGE_BUFFER, stages, Payload, count>;
    template<std::uint32_t binding, ShaderFlag stages, std::uint32_t count = 1>
    using TextureBinding = DescriptorBinding<binding, graphic_resource::Type::COMBINED_IMAGE_SAMPLER, stages, void, count>;

    template<typename... Bindings_>
    struct DescriptorLayout {
        using Layout = DescriptorLayout;
        using Bindings = std::tuple<Bindings_...>;
        static constexpr std::array<graphic_resource::BindingPoint, sizeof...(Bindings_)> POINTS{Bindings_::POINT...};
        static constexpr size_t BINDING_COUNT = sizeof...(Bindings_);
        static constexpr std::array<const BlockType*, BINDING_COUNT> MEMORY_TYPES{Bindings_::MEMORY_LAYOUT...};
        static constexpr std::array<size_t, BINDING_COUNT> PAYLOAD_SIZES{Bindings_::PAYLOAD_SIZE...};
        static consteval bool IsValid();
        static_assert(IsValid(), "Duplicate typed descriptor binding");
        template<std::uint32_t binding>
        using Binding = typename detail::FindDescriptor<binding, Bindings_...>::Type;
        static constexpr size_t BindingIndex(std::uint32_t binding);
    };

    template<std::uint32_t set, typename Layout_>
    struct DescriptorSetBinding : Layout_ {
        using Layout = Layout_;
        static constexpr std::uint32_t SET = set;
        static_assert(set < UINT32_MAX, "Invalid typed descriptor set index");
    };
    template<std::uint32_t set, typename... Bindings>
    using DescriptorSet = DescriptorSetBinding<set, DescriptorLayout<Bindings...>>;

    template<typename... Sets_>
    struct ResourceSchema {
        using Sets = std::tuple<Sets_...>;
        static constexpr size_t COUNT = sizeof...(Sets_);
        static constexpr std::array<std::uint32_t, COUNT> SET_NUMBERS{Sets_::SET...};
        static constexpr std::uint32_t SET_COUNT = std::max({0u, (Sets_::SET + 1)...});
        static constexpr bool EMPTY = COUNT == 0;
        static consteval auto MakeRequirements();
        static constexpr auto REQUIREMENTS = MakeRequirements();
        static consteval bool IsValid();
        static_assert(IsValid(), "Duplicate typed descriptor set");
        template<size_t index>
        using Set = std::tuple_element_t<index, Sets>;
        void injection(PipelineLayoutEditAccessor layout) const { detail::inject_resource_schema(layout, REQUIREMENTS, SET_COUNT); }
    };

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
        size_t size() const { return _count; }
        size_t byte_offset() const { return _value.offset; }
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
        using Storage = Buffer<USAGE | MemoryType::TRANSFER_DESTINATION, property>;
        Storage _buffer;
        size_t _count;

    public:
        static constexpr auto KIND = kind;
        TypedResourceBuffer(LogicalDeviceAccessor device, std::span<const Payload> values)
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        TypedResourceBuffer(LogicalDeviceAccessor device, const Payload& value)
        requires(contain<property, MemoryProperty::HOST_VISIBLE>())
            : TypedResourceBuffer(device, std::span<const Payload>(&value, 1)) {}
        TypedResourceBuffer(LogicalDeviceAccessor device, CommandManager manager, std::span<const Payload> values)
        requires(!contain<property, MemoryProperty::HOST_VISIBLE>())
            : _buffer(device, manager, values.data(), values.size()), _count(values.size()) {}
        Storage& backend() { return _buffer; }
        const Storage& backend() const { return _buffer; }
        size_t size() const { return _count; }
        TypedResourceBufferView<Payload_, kind> view(size_t first, size_t count) const;
        void copy(std::span<const Payload> values)
        requires(contain<property, MemoryProperty::HOST_VISIBLE>());
        detail::DescriptorValue descriptor() const { return {_buffer.data_, {}, {}, _count * sizeof(Payload)}; }
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
        TypedImageResource(ImageAccessor image, SamplerAccessor sampler)
        requires(kind == graphic_resource::Type::COMBINED_IMAGE_SAMPLER)
            : _value{{}, image.data_, sampler.data_} {}
        explicit TypedImageResource(ImageAccessor image)
        requires(kind == graphic_resource::Type::SAMPLED_IMAGE || kind == graphic_resource::Type::STORAGE_IMAGE)
            : _value{{}, image.data_, {}} {}
        explicit TypedImageResource(SamplerAccessor sampler)
        requires(kind == graphic_resource::Type::SAMPLER)
            : _value{{}, {}, sampler.data_} {}
        detail::DescriptorValue descriptor() const { return _value; }
    };
    using TypedTexture = TypedImageResource<graphic_resource::Type::COMBINED_IMAGE_SAMPLER>;
    using TypedSampledImage = TypedImageResource<graphic_resource::Type::SAMPLED_IMAGE>;
    using TypedStorageImage = TypedImageResource<graphic_resource::Type::STORAGE_IMAGE>;
    using TypedSampler = TypedImageResource<graphic_resource::Type::SAMPLER>;

    template<typename Spec_>
    class TypedDescriptorSet {
        GraphicResource _resource;
        std::shared_ptr<std::array<bool, Spec_::BINDING_COUNT>> _initialized;
        LogicalDeviceAccessor _device;

    public:
        using Spec = Spec_;
        using Layout = typename Spec::Layout;
        TypedDescriptorSet(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager);
        const GraphicResource& backend() const { return _resource; }
        LogicalDeviceAccessor device() const { return _device; }
        void require_complete() const;
        template<std::uint32_t binding, typename Value>
        requires(detail::descriptor_matches<typename Spec::template Binding<binding>, Value>() && Spec::template Binding<binding>::POINT.amount == 1)
        void write(const Value& value);
        template<std::uint32_t binding, typename Value, size_t count>
        requires(detail::descriptor_matches<typename Spec::template Binding<binding>, Value>() && Spec::template Binding<binding>::POINT.amount == count)
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
    TypedResourceBufferView<Payload_, kind> TypedResourceBufferView<Payload_, kind>::view(size_t first, size_t count) const {
        auto value = _value;
        value.offset = detail::checked_view_offset(value.offset, first, count, _count, sizeof(Payload));
        value.bytes = count * sizeof(Payload);
        return TypedResourceBufferView(value, count);
    }
    template<typename Payload_, graphic_resource::Type kind, MemoryProperty property>
    TypedResourceBufferView<Payload_, kind> TypedResourceBuffer<Payload_, kind, property>::view(size_t first, size_t count) const {
        auto value = descriptor();
        value.offset = detail::checked_view_offset(0, first, count, _count, sizeof(Payload));
        value.bytes = count * sizeof(Payload);
        return TypedResourceBufferView<Payload_, kind>(value, count);
    }
    template<typename... BindingTypes>
    consteval bool DescriptorLayout<BindingTypes...>::IsValid() {
        for (size_t i = 0; i < POINTS.size(); ++i)
            for (size_t j = 0; j < i; ++j)
                if (POINTS[i].binding == POINTS[j].binding)
                    return false;
        return true;
    }
    template<typename... BindingTypes>
    constexpr size_t DescriptorLayout<BindingTypes...>::BindingIndex(std::uint32_t binding) {
        for (size_t i = 0; i < POINTS.size(); ++i)
            if (POINTS[i].binding == binding)
                return i;
        throw std::invalid_argument("Typed descriptor binding not found");
    }
    template<typename... SetTypes>
    consteval auto ResourceSchema<SetTypes...>::MakeRequirements() {
        std::array<detail::DescriptorRequirement, (SetTypes::BINDING_COUNT + ... + 0)> result{};
        size_t index = 0;
        auto add = [&]<typename Set>() {
            for (size_t i = 0; i < Set::POINTS.size(); ++i)
                result[index++] = {Set::SET, Set::POINTS[i], Set::MEMORY_TYPES[i], Set::PAYLOAD_SIZES[i]};
        };
        (add.template operator()<SetTypes>(), ...);
        return result;
    }
    template<typename... SetTypes>
    consteval bool ResourceSchema<SetTypes...>::IsValid() {
        constexpr std::array<std::uint32_t, sizeof...(SetTypes)> sets{SetTypes::SET...};
        for (size_t i = 0; i < sets.size(); ++i)
            for (size_t j = 0; j < i; ++j)
                if (sets[i] == sets[j])
                    return false;
        return true;
    }
    template<typename PayloadTag, graphic_resource::Type kind, MemoryProperty property>
    TypedResourceBuffer<PayloadTag, kind, property>::TypedResourceBuffer(LogicalDeviceAccessor device, std::span<const Payload> values)
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
        : _buffer(device, helper::checked_buffer_array_size<Payload>(values.size())), _count(values.size()) {
        copy(values);
    }
    template<typename PayloadTag, graphic_resource::Type kind, MemoryProperty property>
    void TypedResourceBuffer<PayloadTag, kind, property>::copy(std::span<const Payload> values)
    requires(contain<property, MemoryProperty::HOST_VISIBLE>())
    {
        if (values.size() != _count)
            throw std::invalid_argument("Typed descriptor buffer update must preserve record count");
        if (!_buffer.copy(values.data(), values.size()))
            throw std::runtime_error("Typed descriptor buffer upload failed");
    }
    template<typename Spec>
    TypedDescriptorSet<Spec>::TypedDescriptorSet(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager)
        : _initialized(std::make_shared<std::array<bool, Spec::BINDING_COUNT>>()), _device(device) {
        graphic_resource::BindingLayout layout(device, std::vector<graphic_resource::BindingPoint>(Spec::POINTS.begin(), Spec::POINTS.end()));
        auto [pools, resources] = manager.allocate({layout});
        if (resources.size() != 1)
            throw std::runtime_error("Typed descriptor allocation failed");
        _resource = resources.front();
    }
    template<typename Spec>
    void TypedDescriptorSet<Spec>::require_complete() const {
        if (!std::ranges::all_of(*_initialized, [](bool value) { return value; }))
            throw std::logic_error("Typed descriptor set has uninitialized bindings");
    }
    template<typename Spec>
    template<std::uint32_t binding, typename Value>
    requires(detail::descriptor_matches<typename Spec::template Binding<binding>, Value>() && Spec::template Binding<binding>::POINT.amount == 1)
    void TypedDescriptorSet<Spec>::write(const Value& value) {
        const auto descriptor = value.descriptor();
        detail::write_descriptor(_resource, Spec::template Binding<binding>::POINT, std::span(&descriptor, 1));
        (*_initialized)[Spec::BindingIndex(binding)] = true;
    }
    template<typename Spec>
    template<std::uint32_t binding, typename Value, size_t count>
    requires(detail::descriptor_matches<typename Spec::template Binding<binding>, Value>() && Spec::template Binding<binding>::POINT.amount == count)
    void TypedDescriptorSet<Spec>::write(const std::array<Value, count>& values) {
        std::array<detail::DescriptorValue, count> descriptors{};
        for (size_t i = 0; i < count; ++i)
            descriptors[i] = values[i].descriptor();
        detail::write_descriptor(_resource, Spec::template Binding<binding>::POINT, descriptors);
        (*_initialized)[Spec::BindingIndex(binding)] = true;
    }
} // namespace VSL_NAMESPACE::typed
