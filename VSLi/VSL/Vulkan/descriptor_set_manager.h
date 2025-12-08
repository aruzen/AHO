//
// Created by morimoto_hibiki on 2025/10/24.
//

#ifndef AHO_ALL_DESCRIPTOR_SET_MANAGER_H
#define AHO_ALL_DESCRIPTOR_SET_MANAGER_H

#include <utility>

#include "../define.h"

#include "descriptor.hpp"

namespace VSL_NAMESPACE::graphic_resource {
    /*
     * 珍しく仮想関数による抽象化で実装しているが
     * makeなどを仮想関数でポリモーフィズムをおこなわないとAHO側のEngineに影響が出そうだからである
     */

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
}

namespace VSL_NAMESPACE {
    using GraphicResourceManager = graphic_resource::ExactAllocateManager;
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

#endif //AHO_ALL_DESCRIPTOR_SET_MANAGER_H
