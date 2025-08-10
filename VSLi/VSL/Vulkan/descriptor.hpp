//
// Created by morimoto_hibiki on 2025/06/09.
//
#ifndef AHO_ALL_DESCRIPTOR_HPP
#define AHO_ALL_DESCRIPTOR_HPP
#include "../define.h"

#include "device.h"
#include "shader.h"
#include "buffer.h"
#include "image.hpp"

namespace VSL_NAMESPACE {
    constexpr size_t MAX_POOL_SIZE = 10000000;

    enum class ResourceType : unsigned int
    {
        Sampler = 0,
        CombinedImageSampler = 1,
        SampledImage = 2,
        StorageImage = 3,
        UniformTexelBuffer = 4,
        StorageTexelBuffer = 5,
        UniformBuffer = 6,
        StorageBuffer = 7,
        UniformBufferDynamic = 8,
        StorageBufferDynamic = 9,
        InputAttachment = 10,
        InlineUniformBlock = 1000138000,
        AccelerationStructure = 1000150000,
        AccelerationStructureNv = 1000165000,
        SampleWeightImageQcom = 1000440000,
        BlockMatchImageQcom = 1000440001,
        MutableValve = 1000351000,
        PartitionedAccelerationStructureNv = 1000570000,
        MaxEnum = 0x7FFFFFFF
    };

    enum class BindingDestination {
        Graphics = 0,
        Compute = 1,
#ifdef VK_ENABLE_BETA_EXTENSIONS
        ExecutionGraphAmdx = 1000134000,
#endif
        RayTracing = 1000165000,
    };

    struct ResourceBindingPoint {
        size_t binding;
        ResourceType bindingType;
        ShaderType shaderType;
        size_t amount = 1;
    };

    template<typename T>
    concept is_graphic_resource = std::is_convertible_v<T*, vsl::BufferAccessor*> || std::is_convertible_v<T*, vsl::Image*>;

    /*
     * GPU上のメモリーに置かれたデータをShaderがどう読み取るかを指定できる
     */
    struct ResourceBindingLayout {
        ResourceBindingLayout(LogicalDeviceAccessor device, ResourceType bindingType, ShaderType shaderType);
        ResourceBindingLayout(const LogicalDeviceAccessor& device, size_t binding, ResourceType bindingType, ShaderType shaderType, size_t amount = 1);
        ResourceBindingLayout(LogicalDeviceAccessor device, std::initializer_list<ResourceBindingPoint> points);

        [[nodiscard]] std::vector<ResourceBindingPoint> getBindingPoints() const;

        std::shared_ptr<_impl::ResourceBindingLayout_impl> _data;
    };

    struct GraphicResourcePool;
    struct GraphicResource;
    struct GraphicResourceManager {
        GraphicResourceManager(LogicalDeviceAccessor device);
        GraphicResourceManager(std::shared_ptr<_impl::GraphicResourceManager_impl> impl);

        GraphicResourcePool make(std::map<ResourceType, size_t> count, size_t other = 0);
        std::tuple<GraphicResourcePool, std::vector<GraphicResource>> make(std::vector<ResourceBindingLayout> layouts);
        template<is_graphic_resource T>
        std::tuple<GraphicResourcePool, std::vector<GraphicResource>> make(std::vector<ResourceBindingLayout> layouts, std::vector<T> buffers);
        template<is_graphic_resource T>
        std::tuple<GraphicResourcePool, std::vector<GraphicResource>> make(std::vector<ResourceBindingLayout> layouts, std::vector<T> buffers, std::vector<size_t > bindings);

        std::shared_ptr<_impl::GraphicResourceManager_impl> _data;

        void destroy(const GraphicResourcePool& pool);
    };

    struct GraphicResourcePool {
        std::shared_ptr<_impl::GraphicResourcePool_impl> _data;

        GraphicResource bind(ResourceBindingLayout layout);
        std::vector<GraphicResource> bind(std::vector<ResourceBindingLayout> layout);

        GraphicResourceManager getManager();
    };

    struct GraphicResource {
        std::shared_ptr<_impl::GraphicResource_impl> _data;
        GraphicResourcePool getPool();

        // ここらへんのTもいつかtypename ...Argsに置き換えないとなぁ
        void update(vsl::BufferAccessor* buffer, size_t binding = 0, std::optional<ResourceType> type = std::nullopt , size_t offset = 0, std::optional<size_t> size = std::nullopt);
        void update(vsl::Image* image, size_t binding = 0, std::optional<ResourceType> type = std::nullopt , size_t offset = 0, std::optional<size_t> size = std::nullopt);
        template<is_graphic_resource T>
        void update(T t, size_t binding = 0, std::optional<ResourceType> type = std::nullopt , size_t offset = 0, std::optional<size_t> size = std::nullopt);
        template<is_graphic_resource T>
        void update(T t, BufferAccessor::LocalBufferHolder holder, size_t binding = 0, std::optional<ResourceType> type = std::nullopt);
    };
}

// ==================================================================================


template<vsl::is_graphic_resource T>
std::tuple<vsl::GraphicResourcePool, std::vector<vsl::GraphicResource>>
vsl::GraphicResourceManager::make(std::vector<ResourceBindingLayout> layouts,
                                  std::vector<T> buffers) {
    auto [pool, resources] = make(layouts);

    for (size_t i = 0; i < resources.size() && i < buffers.size(); i++) {
        resources[i].update(buffers[i], (size_t)0);
    }
    return {pool, resources};
}

template<vsl::is_graphic_resource T>
std::tuple<vsl::GraphicResourcePool, std::vector<vsl::GraphicResource>>
vsl::GraphicResourceManager::make(std::vector<ResourceBindingLayout> layouts, std::vector<T> buffers,
                                  std::vector<size_t> bindings) {
    auto [pool, resources] = make(layouts);

    for (size_t i = 0; i < resources.size() && i < buffers.size(); i++) {
        if (i < bindings.size())
            resources[i].update(buffers[i], bindings[i]);
        else
            resources[i].update(buffers[i], 0);
    }

    return {pool, resources};
}


template<vsl::is_graphic_resource T>
void vsl::GraphicResource::update(T t, vsl::BufferAccessor::LocalBufferHolder holder, size_t binding, std::optional<ResourceType> type) {
    update(t, *holder.parent, binding, type, holder.offset, holder.size);
}

template<vsl::is_graphic_resource T>
void vsl::GraphicResource::update(T t, size_t binding, std::optional<ResourceType> type, size_t offset, std::optional<size_t> size) {
    if constexpr (std::is_convertible_v<T*, vsl::BufferAccessor*>)
        update((vsl::BufferAccessor*)&t, binding, type, offset, size);
    else if constexpr (std::is_convertible_v<T*, vsl::Image*>)
            update((vsl::Image*)&t, binding, type, offset, size);
}

#endif //AHO_ALL_DESCRIPTOR_HPP
