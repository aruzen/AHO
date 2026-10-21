//
// Created by morimoto_hibiki on 2025/06/09.
//
#ifndef AHO_ALL_DESCRIPTOR_HPP
#define AHO_ALL_DESCRIPTOR_HPP

#include <utility>

#include "../define.hpp"

#include "device.hpp"
#include "shader.hpp"
#include "buffer.hpp"
#include "image.hpp"
#include "sampler.hpp"

namespace VSL_NAMESPACE::graphic_resource {
    constexpr size_t MAX_POOL_SIZE = 200;

    enum class Type : unsigned int {
        SAMPLER = 0,
        COMBINED_IMAGE_SAMPLER = 1,
        SAMPLED_IMAGE = 2,
        STORAGE_IMAGE = 3,
        UNIFORM_TEXEL_BUFFER = 4,
        STORAGE_TEXEL_BUFFER = 5,
        UNIFORM_BUFFER = 6,
        STORAGE_BUFFER = 7,
        UNIFORM_BUFFER_DYNAMIC = 8,
        STORAGE_BUFFER_DYNAMIC = 9,
        INPUT_ATTACHMENT = 10,
        INLINE_UNIFORM_BLOCK = 1000138000,
        ACCELERATION_STRUCTURE = 1000150000,
        ACCELERATION_STRUCTURE_NV = 1000165000,
        SAMPLE_WEIGHT_IMAGE_QCOM = 1000440000,
        BLOCK_MATCH_IMAGE_QCOM = 1000440001,
        MUTABLE_VALVE = 1000351000,
        PARTITIONED_ACCELERATION_STRUCTURE_NV = 1000570000,
        MAX_ENUM = 0x7FFFFFFF
    };

    enum class BindingDestination {
        GRAPHICS = 0,
        COMPUTE = 1,
#ifdef VK_ENABLE_BETA_EXTENSIONS
        EXECUTION_GRAPH_AMDX = 1000134000,
#endif
        RAY_TRACING = 1000165000,
    };

    struct BindingPoint {
        std::uint32_t binding;
        Type binding_type;
        ShaderFlag shader_type;
        std::uint32_t amount = 1;

        constexpr auto operator<=>(const BindingPoint&) const = default;
    };

    template<typename T>
    concept can_convert_graphic_resource = std::is_convertible_v<T*, VSL_NAMESPACE::BufferAccessor*> ||
                                           std::is_convertible_v<T*, VSL_NAMESPACE::ImageAccessor*>;

    /*
     * GPU上のメモリーに置かれたデータをShaderがどう読み取るかを指定できる
     */
    struct BindingLayout {
        BindingLayout(LogicalDeviceAccessor device, Type binding_type, ShaderFlag shader_type);

        BindingLayout(LogicalDeviceAccessor device, size_t binding, Type binding_type, ShaderFlag shader_type,
                      size_t amount = 1);

        BindingLayout(LogicalDeviceAccessor device, std::initializer_list<BindingPoint> points);

        BindingLayout(LogicalDeviceAccessor device, const std::vector<BindingPoint>& points);

        [[nodiscard]] std::vector<BindingPoint> get_binding_points() const;

        std::shared_ptr<impl::ResourceBindingLayoutImpl> data_;
    };

    struct ManagerInterface;
    struct Resource;
    struct Pool {
        std::shared_ptr<impl::GraphicResourcePoolImpl> data_;

        std::tuple<bool, Resource> bind(BindingLayout layout);

        std::tuple<bool, std::vector<Resource>> bind(size_t n, BindingLayout layout);

        std::tuple<bool, std::vector<Resource>> bind(std::vector<BindingLayout> layout);

        ManagerInterface* get_manager();
    };

    struct Resource {
        std::shared_ptr<impl::GraphicResourceImpl> data_;

        Pool get_pool();

        // ここらへんのTもいつかtypename ...Argsに置き換えたいかも
        void update(VSL_NAMESPACE::BufferAccessor* buffer, size_t binding = 0, std::optional<Type> type = std::nullopt,
                    size_t offset = 0, std::optional<size_t> size = std::nullopt);

        void update(VSL_NAMESPACE::ImageAccessor image, size_t binding = 0, std::optional<Type> type = std::nullopt);

        void update(VSL_NAMESPACE::ImageAccessor image, SamplerAccessor sampler, size_t binding = 0,
                    std::optional<Type> type = std::nullopt);

        template<can_convert_graphic_resource T>
        void update(T t, size_t binding = 0, std::optional<Type> type = std::nullopt, size_t offset = 0,
                    std::optional<size_t> size = std::nullopt);

        template<can_convert_graphic_resource T>
        void update(T t, BufferAccessor::LocalBufferHolder holder, size_t binding = 0,
                    std::optional<Type> type = std::nullopt);
    };
} // namespace VSL_NAMESPACE::graphic_resource

namespace VSL_NAMESPACE {
    using GraphicResourcePool = graphic_resource::Pool;
    using GraphicResource = graphic_resource::Resource;
} // namespace VSL_NAMESPACE

// ==================================================================================

template<VSL_NAMESPACE::graphic_resource::can_convert_graphic_resource T>
void VSL_NAMESPACE::GraphicResource::update(T t, VSL_NAMESPACE::BufferAccessor::LocalBufferHolder holder,
                                            size_t binding, std::optional<VSL_NAMESPACE::graphic_resource::Type> type) {
    update(t, binding, type, holder.offset, holder.size);
}

template<VSL_NAMESPACE::graphic_resource::can_convert_graphic_resource T>
void VSL_NAMESPACE::GraphicResource::update(T t, size_t binding, std::optional<VSL_NAMESPACE::graphic_resource::Type> type, size_t offset, std::optional<size_t> size) {
    if constexpr (std::is_convertible_v<T*, VSL_NAMESPACE::BufferAccessor*>)
        update((VSL_NAMESPACE::BufferAccessor*)&t, binding, type, offset, size);
    else if constexpr (std::is_convertible_v<T*, VSL_NAMESPACE::ImageAccessor*>)
        update(static_cast<VSL_NAMESPACE::ImageAccessor>(t), binding, type);
}

#endif //AHO_ALL_DESCRIPTOR_HPP
