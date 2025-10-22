//
// Created by morimoto_hibiki on 2025/06/09.
//
#ifndef AHO_ALL_DESCRIPTOR_HPP
#define AHO_ALL_DESCRIPTOR_HPP

#include <utility>

#include "../define.h"

#include "device.h"
#include "shader.h"
#include "buffer.h"
#include "image.hpp"
#include "sampler.h"

namespace VSL_NAMESPACE::graphic_resource {
    constexpr size_t MAX_POOL_SIZE = 10000000;

    enum class Type : unsigned int {
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

    struct BindingPoint {
        size_t binding;
        Type bindingType;
        ShaderFlag shaderType;
        size_t amount = 1;
    };

    template<typename T>
    concept can_convert_graphic_resource = std::is_convertible_v<T *, vsl::BufferAccessor *> ||
            std::is_convertible_v<T *, vsl::ImageAccessor *>;

    /*
     * GPU上のメモリーに置かれたデータをShaderがどう読み取るかを指定できる
     */
    struct BindingLayout {
        BindingLayout(LogicalDeviceAccessor device, Type bindingType, ShaderFlag shaderType);

        BindingLayout(LogicalDeviceAccessor device, size_t binding, Type bindingType, ShaderFlag shaderType,
                      size_t amount = 1);

        BindingLayout(LogicalDeviceAccessor device, std::initializer_list<BindingPoint> points);

        BindingLayout(LogicalDeviceAccessor device, const std::vector<BindingPoint>& points);

        [[nodiscard]] std::vector<BindingPoint> getBindingPoints() const;

        std::shared_ptr<_impl::ResourceBindingLayout_impl> _data;
    };

    /*
     * 珍しく仮想関数による抽象化で実装しているが
     * makeなどを仮想関数でポリモーフィズムをおこなわないとAHO側のEngineに影響が出そうだからである
     */
    struct Pool;
    struct Resource;

    struct ManagerInterface {
        // 戻り値のvectorのサイズが同じになることを保証します
        virtual std::tuple<std::vector<Pool>, std::vector<Resource>>
        allocate(std::vector<BindingLayout> layouts) = 0;

        template<can_convert_graphic_resource T>
        std::tuple<std::vector<Pool>, std::vector<Resource>>
        allocate(std::vector<BindingLayout> layouts, std::vector<T> buffers);

        template<can_convert_graphic_resource T>
        std::tuple<std::vector<Pool>, std::vector<Resource>>
        allocate(std::vector<BindingLayout> layouts, std::vector<T> buffers, std::vector<size_t> bindings);

        Pool make(std::map<Type, size_t> count, size_t other = 0);

        std::tuple<Pool, std::vector<Resource>>
        make(std::vector<BindingLayout> layouts, std::map<Type, size_t> count = {}, size_t other = 0);

        template<can_convert_graphic_resource T>
        std::tuple<Pool, std::vector<Resource>>
        make(std::vector<BindingLayout> layouts, std::vector<T> buffers);

        template<can_convert_graphic_resource T>
        std::tuple<Pool, std::vector<Resource>>
        make(std::vector<BindingLayout> layouts, std::vector<T> buffers, std::vector<size_t> bindings);

        std::shared_ptr<_impl::GraphicResourceManager_impl> _data;

        virtual void destroy(const Pool &pool) = 0;
    };

    /*
     * allocateがLayoutごとにPoolを取ることを保証します。
     * 必要がなくなった時にPoolを解放しても他のリソースには影響がありません。
     * 一時的に使用するようなリソースに使用してください。
     */
    struct ExactAllocateManager : public ManagerInterface {
        ExactAllocateManager(LogicalDeviceAccessor device);

        std::tuple<std::vector<Pool>, std::vector<Resource>> allocate(std::vector<BindingLayout> layouts) override;

        void destroy(const Pool &pool) override;
    };

    /*
     * 最初に大きめのPoolを作った後allocateではそのPoolから割り当てられます。
     * Poolの解放は慎重に行ってください。
     */
    struct OverAllocateManager : public ManagerInterface {
        std::map<Type, size_t> size;
        size_t other;
        std::vector<Pool> pools;

        OverAllocateManager(LogicalDeviceAccessor device);
        OverAllocateManager(LogicalDeviceAccessor device, std::map<Type, size_t> size, size_t other = 0);

        std::tuple<std::vector<Pool>, std::vector<Resource>> allocate(std::vector<BindingLayout> layouts) override;

        void destroy(const Pool &pool) override;
    };

    struct Pool {
        std::shared_ptr<_impl::GraphicResourcePool_impl> _data;

        std::tuple<bool, Resource> bind(BindingLayout layout);

        std::tuple<bool, std::vector<Resource>> bind(std::vector<BindingLayout> layout);

        ManagerInterface *getManager();
    };

    struct Resource {
        std::shared_ptr<_impl::GraphicResource_impl> _data;

        Pool getPool();

        // ここらへんのTもいつかtypename ...Argsに置き換えたいかも
        void update(vsl::BufferAccessor* buffer, size_t binding = 0, std::optional<Type> type = std::nullopt,
                    size_t offset = 0, std::optional<size_t> size = std::nullopt);

        void update(vsl::ImageAccessor image, size_t binding = 0, std::optional<Type> type = std::nullopt,
                    size_t offset = 0,
                    std::optional<size_t> size = std::nullopt);

        void update(vsl::ImageAccessor image, Sampler sampler, size_t binding = 0,
                    std::optional<Type> type = std::nullopt,
                    size_t offset = 0,
                    std::optional<size_t> size = std::nullopt);

        template<can_convert_graphic_resource T>
        void update(T t, size_t binding = 0, std::optional<Type> type = std::nullopt, size_t offset = 0,
                    std::optional<size_t> size = std::nullopt);

        template<can_convert_graphic_resource T>
        void update(T t, BufferAccessor::LocalBufferHolder holder, size_t binding = 0,
                    std::optional<Type> type = std::nullopt);
    };
}

namespace VSL_NAMESPACE {
    using GraphicResourceManager = graphic_resource::ExactAllocateManager;
    using GraphicResourcePool = graphic_resource::Pool;
    using GraphicResource = graphic_resource::Resource;
}

// ==================================================================================

template<vsl::graphic_resource::can_convert_graphic_resource T>
std::tuple<std::vector<vsl::graphic_resource::Pool>, std::vector<vsl::graphic_resource::Resource>>
vsl::graphic_resource::ManagerInterface::allocate(std::vector<BindingLayout> layouts, std::vector<T> buffers) {
    auto [pools, resources] = allocate(std::move(layouts));

    for (size_t i = 0; i < resources.size() && i < buffers.size(); i++) {
        resources[i].update(buffers[i], (size_t) 0);
    }
    return {pools, resources};
}


template<vsl::graphic_resource::can_convert_graphic_resource T>
std::tuple<std::vector<vsl::graphic_resource::Pool>, std::vector<vsl::graphic_resource::Resource>>
vsl::graphic_resource::ManagerInterface::allocate(std::vector<BindingLayout> layouts, std::vector<T> buffers,
                                                  std::vector<size_t> bindings) {
    auto [pools, resources] = allocate(std::move(layouts));

    for (size_t i = 0; i < resources.size() && i < buffers.size(); i++) {
        if (i < bindings.size())
            resources[i].update(buffers[i], bindings[i]);
        else
            resources[i].update(buffers[i], 0);
    }

    return {pools, resources};
}

template<vsl::graphic_resource::can_convert_graphic_resource T>
std::tuple<vsl::graphic_resource::Pool, std::vector<vsl::graphic_resource::Resource>>
vsl::graphic_resource::ManagerInterface::make(std::vector<BindingLayout> layouts, std::vector<T> buffers) {
    auto [pool, resources] = make(layouts);

    for (size_t i = 0; i < resources.size() && i < buffers.size(); i++) {
        resources[i].update(buffers[i], (size_t) 0);
    }
    return {pool, resources};
}

template<vsl::graphic_resource::can_convert_graphic_resource T>
std::tuple<vsl::GraphicResourcePool, std::vector<vsl::GraphicResource>>
vsl::graphic_resource::ManagerInterface::make(std::vector<BindingLayout> layouts, std::vector<T> buffers,
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


template<vsl::graphic_resource::can_convert_graphic_resource T>
void vsl::GraphicResource::update(T t, vsl::BufferAccessor::LocalBufferHolder holder,
                                  size_t binding, std::optional<Type> type) {
    update(t, *holder.parent, binding, type, holder.offset, holder.size);
}

template<vsl::graphic_resource::can_convert_graphic_resource T>
void
vsl::GraphicResource::update(T t, size_t binding, std::optional<Type> type, size_t offset, std::optional<size_t> size) {
    if constexpr (std::is_convertible_v<T *, vsl::BufferAccessor *>)
        update((vsl::BufferAccessor*) &t, binding, type, offset, size);
    else if constexpr (std::is_convertible_v<T *, vsl::ImageAccessor *>)
        update((vsl::ImageAccessor) &t, binding, type, offset, size);
}

#endif //AHO_ALL_DESCRIPTOR_HPP
